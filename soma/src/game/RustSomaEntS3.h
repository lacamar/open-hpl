#ifndef SOMA_RUST_ENT_S3_H
#define SOMA_RUST_ENT_S3_H

// C ABI of rust/src/somaent/s3.rs

#include <cstddef>
#include <cstdint>

struct OhplS3Ids;

struct OhplS3Id
{
	uint8_t a;
	int32_t b;
	int32_t c;
};

struct OhplS3IdHost
{
	void *ctx;
	bool (*alive)(void *, size_t, const unsigned char *, size_t, int);
	int (*creation_id)(void *, size_t);
	size_t (*cast_entity)(void *, size_t, const unsigned char *, size_t);
	size_t (*cast_joint)(void *, size_t, const unsigned char *, size_t);
};

struct OhplS3LiquidBodyIn
{
	bool collide, active, has_mass, is_character, inside, no_gravity_underwater, buoyancy_active, owner_interacted, waves_near, surface_moved;
	float body_max_y, center_y, ext_y;
};

struct OhplS3LiquidBodyPlan
{
	bool skip, exit_callback, buoyancy_off, init_buoyancy, wave_surface, reset_surface;
	int8_t underwater, gravity;
	float max_speed;
};

extern "C"
{
	OhplS3Ids *ohpl_s3_ids_new(int seed);
	void ohpl_s3_ids_free(OhplS3Ids *);
	void ohpl_s3_ids_clear(OhplS3Ids *);
	void ohpl_s3_ids_register_bodies(OhplS3Ids *, const size_t *bodies, size_t n);
	void ohpl_s3_object_id(OhplS3Ids *, OhplS3IdHost *, size_t obj, const unsigned char *ty, size_t tyn, OhplS3Id *out);
	size_t ohpl_s3_object_from_id(OhplS3Ids *, OhplS3IdHost *, const OhplS3Id *, const unsigned char *ty, size_t tyn);
	void ohpl_s3_liquid_body_plan(const OhplS3LiquidBodyIn *, OhplS3LiquidBodyPlan *);
}

#endif
