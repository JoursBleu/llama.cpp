#include "models.h"

#include "llama-model.h"
#include "llama-hparams.h"

#include "ggml.h"

#include <cstring>
#include <cmath>

// EAGLE2 graph builder
// Architecture: fc(concat(token_embd, hidden_state)) -> 1 decoder layer -> lm_head
//
// Input flow:
//   1. Token from batch -> embed via tok_embd
//   2. Hidden state from target model (or prenorm from prev step) via inp_g_embeddings
//   3. concat(embd, hidden) -> fc -> decoder input
//   4. 1 standard LLaMA decoder layer (attention + SwiGLU FFN)
//   5. Output: logits + prenorm (for autoregressive)

llm_build_eagle2::llm_build_eagle2(const llama_model & model, const llm_graph_params & params)
    : llm_graph_context(params) {

    const int64_t n_embd_head = hparams.n_embd_head_v();

    GGML_ASSERT(n_embd_head == hparams.n_embd_head_k());
    GGML_ASSERT(n_layer == 1);  // EAGLE2 has only one decoder layer

    ggml_tensor * cur;
    ggml_tensor * inpL;

    // 1. Token embeddings from batch
    // Use EAGLE2's own tok_embd if available, else target's
    ggml_tensor * token_embd_eagle2 = (model.tok_embd != nullptr) ? model.tok_embd : model.target_tok_embd;
    GGML_ASSERT(token_embd_eagle2 != nullptr && "EAGLE2 requires token embeddings (own or from target model)");
    ggml_tensor * inp_embd = build_inp_embd(token_embd_eagle2);
    cb(inp_embd, "inp_embd", -1);

    // 2. g_embeddings: hidden state from target or prenorm from previous step
    ggml_tensor * inp_g = ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, n_embd, n_tokens);
    ggml_set_input(inp_g);
    cb(inp_g, "inp_g_embeddings", -1);

    // 3. Concatenate token embeddings and hidden state: [2*n_embd, n_tokens]
    ggml_tensor * concat = ggml_concat(ctx0, inp_embd, inp_g, 0);
    cb(concat, "eagle2_concat", -1);

    // 4. FC projection: [2*n_embd, n_tokens] -> [n_embd, n_tokens]
    inpL = ggml_mul_mat(ctx0, model.eagle2_fc, concat);
    if (model.eagle2_fc_bias) {
        inpL = ggml_add(ctx0, inpL, model.eagle2_fc_bias);
    }
    cb(inpL, "eagle2_fc_out", -1);

    // inp_pos - contains the positions
    ggml_tensor * inp_pos = build_inp_pos();

    auto * inp_attn = build_attn_inp_kv();

    const float kq_scale = 1.0f/sqrtf(float(n_embd_head));

    ggml_tensor * inp_out_ids = build_inp_out_ids();

    // 5. Single decoder layer (il = 0)
    const int il = 0;
    {
        // Pre-attention norm
        ggml_tensor * inpSA = inpL;
        cur = build_norm(inpL,
                model.layers[il].attn_norm, NULL,
                LLM_NORM_RMS, il);
        cb(cur, "attn_norm", il);

        // Self-attention (standard dimension: input dim = n_embd, not 2*n_embd)
        ggml_tensor * Qcur = build_lora_mm(model.layers[il].wq, cur);
        cb(Qcur, "Qcur", il);

        ggml_tensor * Kcur = build_lora_mm(model.layers[il].wk, cur);
        cb(Kcur, "Kcur", il);

        ggml_tensor * Vcur = build_lora_mm(model.layers[il].wv, cur);
        cb(Vcur, "Vcur", il);

        Qcur = ggml_reshape_3d(ctx0, Qcur, n_embd_head, n_head,    n_tokens);
        Kcur = ggml_reshape_3d(ctx0, Kcur, n_embd_head, n_head_kv, n_tokens);
        Vcur = ggml_reshape_3d(ctx0, Vcur, n_embd_head, n_head_kv, n_tokens);

        // rope freq factors (returns nullptr if not available)
        ggml_tensor * rope_factors = model.get_rope_factors(cparams, il);

        // RoPE
        Qcur = ggml_rope_ext(
                ctx0, Qcur, inp_pos, rope_factors,
                n_rot, rope_type, n_ctx_orig, freq_base, freq_scale,
                ext_factor, attn_factor, beta_fast, beta_slow);
        Kcur = ggml_rope_ext(
                ctx0, Kcur, inp_pos, rope_factors,
                n_rot, rope_type, n_ctx_orig, freq_base, freq_scale,
                ext_factor, attn_factor, beta_fast, beta_slow);

        cb(Qcur, "Qcur_rope", il);
        cb(Kcur, "Kcur_rope", il);

        cur = build_attn(inp_attn,
                model.layers[il].wo, NULL,
                Qcur, Kcur, Vcur, nullptr, nullptr, nullptr, kq_scale, il);

        if (inp_out_ids) {
            cur   = ggml_get_rows(ctx0,   cur, inp_out_ids);
            inpSA = ggml_get_rows(ctx0, inpSA, inp_out_ids);
        }

        // Residual connection
        ggml_tensor * ffn_inp = ggml_add(ctx0, cur, inpSA);
        cb(ffn_inp, "ffn_inp", il);

        // FFN norm
        cur = build_norm(ffn_inp,
                model.layers[il].ffn_norm, NULL,
                LLM_NORM_RMS, il);
        cb(cur, "post_attn_norm", il);

        // SwiGLU FFN
        cur = build_ffn(cur,
                model.layers[il].ffn_up,   NULL, NULL,
                model.layers[il].ffn_gate, NULL, NULL,
                model.layers[il].ffn_down, NULL, NULL,
                NULL,
                LLM_FFN_SILU, LLM_FFN_PAR, il);
        cb(cur, "ffn_out", il);

        // Residual
        cur = ggml_add(ctx0, cur, ffn_inp);
        cb(cur, "eagle2_prenorm", il);

        inpL = cur;
    }

    cur = inpL;

    // Output prenorm state (for next step's g_embeddings in autoregressive generation)
    ggml_set_output(cur);
    res->t_embd = cur;

    // Output norm + lm_head -> logits
    cur = build_norm(cur,
            model.output_norm, NULL,
            LLM_NORM_RMS, -1);
    cb(cur, "result_norm", -1);

    cur = build_lora_mm(model.output, cur);
    cb(cur, "result_output", -1);

    res->t_logits = cur;

    ggml_build_forward_expand(gf, cur);
}
