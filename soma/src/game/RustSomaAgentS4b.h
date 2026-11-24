#ifndef SOMA_RUST_AGENT_S4B_H
#define SOMA_RUST_AGENT_S4B_H

// C ABI of rust/src/somaagent/s4b/ffi.rs

#include <cstddef>
#include <cstdint>

struct OhplS4bIo
{
	uintptr_t p[4];
	int i[8];
	float f[16];
	uint64_t a[4];
	const unsigned char *s[2];
	size_t n[2];
};

extern "C"
{
	void ohpl_s4b_install(int (*apOp)(unsigned, OhplS4bIo *));
	void ohpl_s4b_register(void *apEngine);
	void ohpl_s4b_create(uintptr_t ent, const void *core, const void *names);
	void ohpl_s4b_destroy(uintptr_t ent);
	void ohpl_s4b_update(uintptr_t ent, float dt);
	void ohpl_s4b_update_components(uintptr_t ent, float dt);
	void ohpl_s4b_set_active(uintptr_t ent, bool on);
	uintptr_t ohpl_s4b_body(uintptr_t ent);
	bool ohpl_s4b_get_pos(uintptr_t ent, float *out);
	bool ohpl_s4b_set_pos(uintptr_t ent, const float *p);
	int ohpl_s4b_state(uintptr_t ent);
	void ohpl_s4b_save_extra(uintptr_t ent, float *yaw, bool *senses, bool *det);
	void ohpl_s4b_load_extra(uintptr_t ent, float yaw, bool senses, bool det);
	void ohpl_s4b_save_path(uintptr_t ent, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
	void ohpl_s4b_load_path(uintptr_t ent, const unsigned char *data, size_t len);
	void ohpl_s4b_change_state(uintptr_t ent, int st);
	void ohpl_s4b_send_message(uintptr_t ent, int msg, const float *v, int x);
	void ohpl_s4b_broadcast_sound(const float *pos, float radius, int prio);
	void ohpl_s4b_debug(uintptr_t ent, void *ctx, void (*sink)(void *, const unsigned char *, size_t));
	void ohpl_s4b_glow_each(void *ctx, void (*cb)(void *, uintptr_t, const float *, float, float, float));
}

#endif
