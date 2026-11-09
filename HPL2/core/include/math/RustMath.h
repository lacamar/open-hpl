#ifndef HPL_RUST_MATH_H
#define HPL_RUST_MATH_H

// C ABI of rust/src/math/ffi.rs. Aggregates cross as pointers (cVector3 has a user-provided copy ctor).

#include "math/RustTypes.h"
#include "math/Quaternion.h"
#include "graphics/Color.h"

namespace hpl {
	static_assert(sizeof(cPlanef) == 16 && offsetof(cPlanef, d) == 12, "cPlanef layout");
	static_assert(sizeof(cRect2f) == 16 && sizeof(cRect2l) == 16 && offsetof(cRect2l, h) == 12, "cRect2 layout");
}

namespace hpl {
extern "C" {

int ohpl_math_rand_rectl(int, int);
float ohpl_math_rand_rectf(float, float);
float ohpl_math_fast_random_float(int);
void ohpl_math_rand_rect_vector2f(const cVector2f*, const cVector2f*, cVector2f*);
void ohpl_math_rand_rect_vector3f(const cVector3f*, const cVector3f*, cVector3f*);
void ohpl_math_rand_rect_color(const cColor*, const cColor*, cColor*);
void ohpl_math_randomize(int);
int ohpl_math_get_flag_bit(int);

void ohpl_math_expand_aabb(cVector3f*, cVector3f*, const cVector3f*, const cVector3f*);
bool ohpl_math_check_point_in_rect(const cVector2f*, const cRect2f*);
bool ohpl_math_check_rect_fit_l(const cRect2l*, const cRect2l*);
bool ohpl_math_check_rect_fit_f(const cRect2f*, const cRect2f*);
void ohpl_math_get_clip_rect(const cRect2f*, const cRect2f*, cRect2f*);
bool ohpl_math_check_sphere_line_intersection(const cVector3f*, float, const cVector3f*, const cVector3f*, float*, cVector3f*, float*, cVector3f*);
bool ohpl_math_check_aabb_intersection(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*);
bool ohpl_math_check_aabb_inside(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*);
bool ohpl_math_check_point_in_aabb(const cVector3f*, const cVector3f*, const cVector3f*);
bool ohpl_math_check_point_inside_triangle_2d(const cVector2f*, const cVector3f*);
bool ohpl_math_check_point_same_side_of_edge_2d(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*);
void ohpl_math_get_clip_rect_from_normalized_min_max(const cVector3f*, const cVector3f*, const cVector2l*, cRect2l*);
bool ohpl_math_get_normalized_clip_rect_from_bv(const cVector3f*, const cVector3f*, const cMatrixf*, const cMatrixf*, bool, float, float, float, float, cVector3f*, cVector3f*);
void ohpl_math_get_clip_rect_from_sphere(const cVector3f*, float, const cMatrixf*, float, float, float, const cVector2l*, bool, float, cRect2l*);
bool ohpl_math_check_aabb_line_intersection(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*, cVector3f*, float*);

float ohpl_math_round_float_to_decimals(float, int);
float ohpl_math_get_fraction(float);
float ohpl_math_modulus(float, float);
int ohpl_math_log2_to_int(int);
bool ohpl_math_is_pow2(int);
float ohpl_math_wrap(float, float, float);
float ohpl_math_clamp(float, float, float);
float ohpl_math_get_angle_distance(float, float, float);
float ohpl_math_turn_angle(float, float, float, float);
float ohpl_math_to_rad(float);
float ohpl_math_to_deg(float);
float ohpl_math_interpolate(float, float, float);
float ohpl_math_increase_to(float, float, float);
float ohpl_math_smooth_curve(float);

float ohpl_math_vector2_dist(const cVector2f*, const cVector2f*);
float ohpl_math_vector2_dist_xy(const cVector3f*, const cVector3f*);
float ohpl_math_vector2_dist_xz(const cVector3f*, const cVector3f*);
float ohpl_math_vector2_dist_sqr(const cVector2f*, const cVector2f*);
float ohpl_math_vector2_dist_sqr_xy(const cVector3f*, const cVector3f*);
float ohpl_math_vector2_dist_sqr_xz(const cVector3f*, const cVector3f*);
void ohpl_math_vector2_increase_to(const cVector2f*, const cVector2f*, const cVector2f*, cVector2f*);
float ohpl_math_get_angle_from_points_2d(const cVector2f*, const cVector2f*);
void ohpl_math_get_vector_from_angle_2d(float, float, cVector2f*);
void ohpl_math_get_angle_from_vector(const cVector2f*, float*, float*);
void ohpl_math_project_vector_2d(const cVector2f*, const cVector2f*, cVector2f*);

void ohpl_math_vector3_max_length(const cVector3f*, float, cVector3f*);
void ohpl_math_vector3_min_length(const cVector3f*, float, cVector3f*);
void ohpl_math_vector3_clamp_length(const cVector3f*, float, float, cVector3f*);
void ohpl_math_vector3_angle_distance(const cVector3f*, const cVector3f*, float, cVector3f*);
void ohpl_math_vector3_cross(const cVector3f*, const cVector3f*, cVector3f*);
float ohpl_math_vector3_dot(const cVector3f*, const cVector3f*);
void ohpl_math_vector3_project(const cVector3f*, const cVector3f*, cVector3f*);
void ohpl_math_vector3_reflect(const cVector3f*, const cVector3f*, cVector3f*);
float ohpl_math_vector3_angle(const cVector3f*, const cVector3f*);
void ohpl_math_vector3_unproject(const cVector3f*, const cRect2f*, const cMatrixf*, cVector3f*);
void ohpl_math_vector3_increase_to(const cVector3f*, const cVector3f*, const cVector3f*, cVector3f*);
void ohpl_math_vector3_clamp_to_length(cVector3f*, float);
bool ohpl_math_get_angle_from_points_3d(const cVector3f*, const cVector3f*, cVector3f*);
void ohpl_math_vector3_orthonormalize_basis(const cVector3f*, const cVector3f*, const cVector3f*, cVector3f*, cVector3f*, cVector3f*);
void ohpl_math_get_spherical_coords_from_point_3d(const cVector3f*, const cVector3f*, cVector2f*);
void ohpl_math_get_point_3d_from_spherical_coords(const cVector3f*, float, const cVector2f*, cVector3f*);

float ohpl_math_plane_dot(const cPlanef*, const cPlanef*);
bool ohpl_math_plane_parallel(const cPlanef*, const cPlanef*);
void ohpl_math_transform_plane(const cMatrixf*, const cPlanef*, cPlanef*);
float ohpl_math_plane_to_point_dist(const cPlanef*, const cVector3f*);
void ohpl_math_plane_intersection_line(const cPlanef*, const cPlanef*, cVector3f*, cVector3f*);
bool ohpl_math_plane_intersection_point(const cPlanef*, const cPlanef*, const cPlanef*, cVector3f*);
int ohpl_math_check_plane_sphere_collision(const cPlanef*, const cVector3f*, float);
int ohpl_math_check_plane_aabb_collision(const cPlanef*, const cVector3f*, const cVector3f*);
int ohpl_math_check_plane_aabb_sphere_collision(const cPlanef*, const cVector3f*, const cVector3f*, const cVector3f*, float);
bool ohpl_math_check_plane_line_intersection(const cPlanef*, const cVector3f*, const cVector3f*, cVector3f*, float*);
int ohpl_math_check_points_planes_collision(const cVector3f*, int, const cPlanef*, int);
int ohpl_math_check_points_aabb_planes_collision(const cVector3f*, int, const cVector3f*, const cVector3f*);
bool ohpl_math_check_separating_axis_intersection(const cVector3f*, int, const cPlanef*, int, const cVector3f*, int, const cPlanef*, int);
bool ohpl_math_check_line_tri_mesh_intersection(const cVector3f*, const cVector3f*, const cMatrixf*, const unsigned int*, int, const float*, int, cVector3f*, float*, int*, bool);

float ohpl_math_quaternion_dot(const cQuaternion*, const cQuaternion*);
void ohpl_math_quaternion_mul(const cQuaternion*, const cQuaternion*, cQuaternion*);
void ohpl_math_quaternion_slerp(float, const cQuaternion*, const cQuaternion*, bool, cQuaternion*);
void ohpl_math_matrix_slerp(float, const cMatrixf*, const cMatrixf*, cMatrixf*);
void ohpl_math_matrix_mul(const cMatrixf*, const cMatrixf*, cMatrixf*);
void ohpl_math_matrix_mul_vec(const cMatrixf*, const cVector3f*, cVector3f*);
void ohpl_math_matrix_mul_3x3(const cMatrixf*, const cVector3f*, cVector3f*);
void ohpl_math_matrix_mul_divide_w(const cMatrixf*, const cVector3f*, cVector3f*);
void ohpl_math_matrix_mul_scalar(const cMatrixf*, float, cMatrixf*);
void ohpl_math_matrix_rotate(const cVector3f*, int, cMatrixf*);
void ohpl_math_matrix_rotate_x(float, cMatrixf*);
void ohpl_math_matrix_rotate_y(float, cMatrixf*);
void ohpl_math_matrix_rotate_z(float, cMatrixf*);
void ohpl_math_matrix_scale(const cVector3f*, cMatrixf*);
void ohpl_math_matrix_translate(const cVector3f*, cMatrixf*);
void ohpl_math_matrix_quaternion(const cQuaternion*, cMatrixf*);
void ohpl_math_matrix_to_quaternion(const cMatrixf*, cQuaternion*);
void ohpl_math_matrix_plane_mirror(const cPlanef*, cMatrixf*);
void ohpl_math_matrix_to_euler_angles(const cMatrixf*, cVector3f*);
void ohpl_math_matrix_euler_angle_distance(const cMatrixf*, const cMatrixf*, cVector3f*);
void ohpl_math_matrix_unit_vectors(const cVector3f*, const cVector3f*, const cVector3f*, const cVector3f*, cMatrixf*);
void ohpl_math_projection_matrix_oblique_near_clip_plane(const cMatrixf*, const cPlanef*, cMatrixf*);
void ohpl_math_matrix_perspective_projection(float, float, float, float, bool, cMatrixf*);
void ohpl_math_matrix_orthographic_projection(float, float, const cVector2f*, cMatrixf*);
void ohpl_math_create_tri_tangent_vectors(float*, const unsigned int*, int, const float*, int, const float*, const float*, int);

void ohpl_quaternion_normalize(cQuaternion*);
void ohpl_quaternion_to_rotation_matrix(const cQuaternion*, cMatrixf*);
void ohpl_quaternion_from_rotation_matrix(cQuaternion*, const cMatrixf*);
void ohpl_quaternion_from_angle_axis(cQuaternion*, float, const cVector3f*);
void ohpl_quaternion_add(const cQuaternion*, const cQuaternion*, cQuaternion*);
void ohpl_quaternion_sub(const cQuaternion*, const cQuaternion*, cQuaternion*);
void ohpl_quaternion_mul(const cQuaternion*, const cQuaternion*, cQuaternion*);
void ohpl_quaternion_div(const cQuaternion*, const cQuaternion*, cQuaternion*);
void ohpl_quaternion_scale(const cQuaternion*, float, cQuaternion*);
}
}

#endif
