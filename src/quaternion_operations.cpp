#include <iostream>
#include <string>
#include <vector>
#include <Eigen/Dense>
// #include <Eigen/Geometry>
#include <cmath>
#include "quaternion_operations.hpp"
using namespace Eigen;

// Convert Quaterniond to Vector3d
Vector3d quat_to_vec3(const Quaterniond& q) {
    return Vector3d(q.x(), q.y(), q.z());
}

// Quaternion product
Quaterniond quat_prod(const Quaterniond& q1, const Quaterniond& q2) {
    Quaterniond q3;

    Vector3d v1 = quat_to_vec3(q1);
    Vector3d v2 = quat_to_vec3(q2);

    float dot = v1.dot(v2); // Dot product

    Vector3d cr = v1.cross(v2); // Cross product

    q3.w() = q1.w() * q2.w()- dot;
    q3.x() = q1.w() * q2.x() + q2.w() * q1.x() + cr.x();
    q3.y() = q1.w() * q2.y() + q2.w() * q1.y() + cr.y();
    q3.z() = q1.w() * q2.z() + q2.w() * q1.z() + cr.z();
    return q3;
}



Vector3d quat_to_euler(const Quaterniond& q){
    Vector3d E;
    
    E.x() = atan2f(2*(q.w()*q.x()+q.y() *q.z()), (q.w() *q.w() + q.z() *q.z() - q.x() *q.x() - q.y() *q.y()));
    E.y() = asinf(2*(q.w() *q.y() - q.x()*q.z()));
    E.z()= atan2f(2*(q.w() *q.z() + q.x() *q.y()), q.w()*q.w() + q.x()*q.x() - q.y()*q.y() - q.z()*q.z());

    return E;
}

Quaterniond euler_to_quat(const Vector3d& E){
    Quaterniond q;
    float phi = E.x(), theta = E.y(), psi = E.z();
    Quaterniond q1 = {cosf(phi/2),sinf(phi/2),0,0};
    Quaterniond q2 = {cosf(theta/2),0,sinf(theta/2),0};
    Quaterniond q3 = {cosf(psi/2),0,0,sinf(psi/2)};
    q = quat_prod(q3,quat_prod(q2,q1));
    return q;

}
