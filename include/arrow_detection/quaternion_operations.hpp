#ifndef QUATERNION_OPERATIONS_HPP
#define QUATERNION_OPERATIONS_HPP

#include "Eigen/Dense"
#include "quaternion_operations.hpp"
#include <cmath> 
#include <vector>
#include <Eigen/Geometry>
using namespace Eigen;

// Quaterniond vec3_to_quat(const Vector3d &v) ;
Vector3d quat_to_vec3(const Quaterniond &q) ;
Quaterniond quat_prod(const Quaterniond& q1, const Quaterniond& q2);
// Vector3d quat_rot(const Quaterniond &q, const Vector3d &v);
// Quaterniond quat_mult_f(Quaterniond q1, float a);
Vector3d quat_to_euler(const Quaterniond& q);
Quaterniond euler_to_quat(const Vector3d& E);
Vector3d normalize_vec(Vector3d input);
double normalize_element(double input);


#endif