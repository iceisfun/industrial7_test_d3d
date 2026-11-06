#include "dx.h"
/* d3dx9_43.dll math library: exact analytic results (row-vector convention, left handed). */

#define NEED_MATH() NEED_DLL(X9)

/* ---- identity / construction ------------------------------------------- */
TEST(d3dx_redist_optional_x9_matrix_identity) {
    /* D3DXMatrixIdentity is header-inline in D3DX 9.43: not exported (probed); the exported D3DXMatrixIsIdentity agrees with the local helper */
    NEED_MATH();
    SET_ERR(0); CHECK(GetProcAddress(GetModuleHandleA(X9), "D3DXMatrixIdentity") == NULL);
    D3DXMATRIX m; memset(&m, 0x55, sizeof m);
    D3DXMATRIX *r = dx_matrix_identity(&m);
    CHECK(r == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    BOOL (WINAPI *isid)(const D3DXMATRIX *) = (void *)GetProcAddress(GetModuleHandleA(X9), "D3DXMatrixIsIdentity");
    if (isid) { CHECK(isid(&m) == TRUE); m._12 = 1e-3f; CHECK(isid(&m) == FALSE); }
}
TEST(d3dx_redist_optional_x9_matrix_translation) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX m; D3DXMATRIX *r = F9(D3DXMatrixTranslation)(&m, 1.5f, -2.f, 3.f);
    CHECK(r == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 1.5f,-2,3,1);
}
TEST(d3dx_redist_optional_x9_matrix_scaling) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixScaling));
    D3DXMATRIX m; F9(D3DXMatrixScaling)(&m, 2, 3, 4);
    CHECK_MAT(m, 2,0,0,0, 0,3,0,0, 0,0,4,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_rotation_x) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationX));
    D3DXMATRIX m; F9(D3DXMatrixRotationX)(&m, DX_PI / 2);
    CHECK_MAT(m, 1,0,0,0, 0,0,1,0, 0,-1,0,0, 0,0,0,1);
    F9(D3DXMatrixRotationX)(&m, 0);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_rotation_y) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationY));
    D3DXMATRIX m; F9(D3DXMatrixRotationY)(&m, DX_PI / 2);
    CHECK_MAT(m, 0,0,-1,0, 0,1,0,0, 1,0,0,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_rotation_z) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationZ));
    D3DXMATRIX m; F9(D3DXMatrixRotationZ)(&m, DX_PI / 2);
    CHECK_MAT(m, 0,1,0,0, -1,0,0,0, 0,0,1,0, 0,0,0,1);
    F9(D3DXMatrixRotationZ)(&m, DX_PI);
    CHECK_MAT(m, -1,0,0,0, 0,-1,0,0, 0,0,1,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_rotation_axis_matches_axes) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationAxis)); REQUIRE(F9(D3DXMatrixRotationX)); REQUIRE(F9(D3DXMatrixRotationY)); REQUIRE(F9(D3DXMatrixRotationZ));
    static const D3DXVECTOR3 ax[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    static const float ang[] = { 0.3f, 1.0f, -2.2f, 3.0f };
    for (int a = 0; a < 3; a++) for (int i = 0; i < 4; i++) {
        D3DXMATRIX m1, m2;
        F9(D3DXMatrixRotationAxis)(&m1, &ax[a], ang[i]);
        (a == 0 ? F9(D3DXMatrixRotationX) : a == 1 ? F9(D3DXMatrixRotationY) : F9(D3DXMatrixRotationZ))(&m2, ang[i]);
        CHECKF(dx_mat_near(&m1, (const float *)&m2, 1e-5f), "axis %d angle %g", a, ang[i]);
    }
}
TEST(d3dx_redist_optional_x9_matrix_rotation_axis_unnormalized) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationAxis)); REQUIRE(F9(D3DXMatrixRotationZ));
    D3DXVECTOR3 ax = { 0, 0, 7 }; D3DXMATRIX m1, m2;
    F9(D3DXMatrixRotationAxis)(&m1, &ax, 0.9f); F9(D3DXMatrixRotationZ)(&m2, 0.9f);
    CHECK(dx_mat_near(&m1, (const float *)&m2, 1e-5f));
}

/* ---- multiply / transpose / determinant / inverse ----------------------- */
TEST(d3dx_redist_optional_x9_matrix_multiply_order) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixMultiply)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX s, t, r;
    F9(D3DXMatrixScaling)(&s, 2, 2, 2); F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXMATRIX *p = F9(D3DXMatrixMultiply)(&r, &s, &t);   /* scale first, then translate */
    CHECK(p == &r);
    CHECK_MAT(r, 2,0,0,0, 0,2,0,0, 0,0,2,0, 1,2,3,1);
    F9(D3DXMatrixMultiply)(&r, &t, &s);                    /* translate first, then scale */
    CHECK_MAT(r, 2,0,0,0, 0,2,0,0, 0,0,2,0, 2,4,6,1);
}
TEST(d3dx_redist_optional_x9_matrix_multiply_aliasing) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixMultiply)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX s, t, ref;
    F9(D3DXMatrixScaling)(&s, 2, 3, 4); F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    F9(D3DXMatrixMultiply)(&ref, &s, &t);
    D3DXMATRIX a = s; F9(D3DXMatrixMultiply)(&a, &a, &t);
    CHECK(dx_mat_near(&a, (const float *)&ref, 0));
    D3DXMATRIX b = t; F9(D3DXMatrixMultiply)(&b, &s, &b);
    CHECK(dx_mat_near(&b, (const float *)&ref, 0));
    D3DXMATRIX c = s; F9(D3DXMatrixMultiply)(&c, &c, &c);
    CHECK_MAT(c, 4,0,0,0, 0,9,0,0, 0,0,16,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_multiply_transpose) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixMultiplyTranspose)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX s, t, r;
    F9(D3DXMatrixScaling)(&s, 2, 2, 2); F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    F9(D3DXMatrixMultiplyTranspose)(&r, &s, &t);
    CHECK_MAT(r, 2,0,0,1, 0,2,0,2, 0,0,2,3, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_transpose) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTranspose));
    D3DXMATRIX m, r; float *f = (float *)&m; for (int i = 0; i < 16; i++) f[i] = (float)(i + 1);
    CHECK(F9(D3DXMatrixTranspose)(&r, &m) == &r);
    CHECK_MAT(r, 1,5,9,13, 2,6,10,14, 3,7,11,15, 4,8,12,16);
    F9(D3DXMatrixTranspose)(&m, &m);  /* in place */
    CHECK(dx_mat_near(&m, (const float *)&r, 0));
}
TEST(d3dx_redist_optional_x9_matrix_determinant) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixDeterminant)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX m;
    dx_matrix_identity(&m); CHECKN(F9(D3DXMatrixDeterminant)(&m), 1.f);
    F9(D3DXMatrixScaling)(&m, 2, 3, 4); CHECKN(F9(D3DXMatrixDeterminant)(&m), 24.f);
    F9(D3DXMatrixScaling)(&m, 2, -3, 4); CHECKN(F9(D3DXMatrixDeterminant)(&m), -24.f);
    F9(D3DXMatrixTranslation)(&m, 5, 6, 7); CHECKN(F9(D3DXMatrixDeterminant)(&m), 1.f);
    F9(D3DXMatrixScaling)(&m, 2, 0, 4); CHECKN(F9(D3DXMatrixDeterminant)(&m), 0.f);
}
TEST(d3dx_redist_optional_x9_matrix_inverse_basic) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixInverse)); REQUIRE(F9(D3DXMatrixTranslation)); REQUIRE(F9(D3DXMatrixScaling));
    D3DXMATRIX m, r; float det = -99;
    F9(D3DXMatrixTranslation)(&m, 1, 2, 3);
    CHECK(F9(D3DXMatrixInverse)(&r, &det, &m) == &r);
    CHECKN(det, 1.f);
    CHECK_MAT(r, 1,0,0,0, 0,1,0,0, 0,0,1,0, -1,-2,-3,1);
    F9(D3DXMatrixScaling)(&m, 2, 4, 8);
    CHECK(F9(D3DXMatrixInverse)(&r, NULL, &m) == &r);  /* determinant output is optional */
    CHECK_MAT(r, 0.5f,0,0,0, 0,0.25f,0,0, 0,0,0.125f,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_inverse_singular) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixInverse)); REQUIRE(F9(D3DXMatrixScaling));
    D3DXMATRIX m, r; float det = -99;
    F9(D3DXMatrixScaling)(&m, 1, 0, 1);
    memset(&r, 0x7f, sizeof r);
    D3DXMATRIX *p = F9(D3DXMatrixInverse)(&r, &det, &m);
    CHECK(p == NULL);
    t_note("determinant output for a singular matrix: %g", det);
}
TEST(d3dx_redist_optional_x9_matrix_inverse_roundtrip) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixInverse)); REQUIRE(F9(D3DXMatrixMultiply)); REQUIRE(F9(D3DXMatrixRotationYawPitchRoll)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX r, s, t, a, b, inv, id;
    F9(D3DXMatrixRotationYawPitchRoll)(&r, 0.4f, -0.7f, 1.3f);
    F9(D3DXMatrixScaling)(&s, 1.5f, 2.f, 0.5f); F9(D3DXMatrixTranslation)(&t, 3, -4, 5);
    F9(D3DXMatrixMultiply)(&a, &r, &s); F9(D3DXMatrixMultiply)(&b, &a, &t);
    CHECK(F9(D3DXMatrixInverse)(&inv, NULL, &b) == &inv);
    F9(D3DXMatrixMultiply)(&id, &b, &inv);
    static const float w[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    CHECKF(dx_mat_near(&id, w, 1e-4f), "M*inv(M) is not identity");
}

/* ---- view / projection -------------------------------------------------- */
TEST(d3dx_redist_optional_x9_matrix_lookat_lh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixLookAtLH));
    D3DXVECTOR3 eye = {0,0,-5}, at = {0,0,0}, up = {0,1,0}; D3DXMATRIX m;
    CHECK(F9(D3DXMatrixLookAtLH)(&m, &eye, &at, &up) == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,5,1);
}
TEST(d3dx_redist_optional_x9_matrix_lookat_rh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixLookAtRH));
    D3DXVECTOR3 eye = {0,0,5}, at = {0,0,0}, up = {0,1,0}; D3DXMATRIX m;
    CHECK(F9(D3DXMatrixLookAtRH)(&m, &eye, &at, &up) == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-5,1);
}
TEST(d3dx_redist_optional_x9_matrix_lookat_axis_swap) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixLookAtLH));
    /* looking down +x from the origin, y up: view-space z = world x, view-space x = -world z */
    D3DXVECTOR3 eye = {0,0,0}, at = {1,0,0}, up = {0,1,0}; D3DXMATRIX m;
    F9(D3DXMatrixLookAtLH)(&m, &eye, &at, &up);
    CHECK_MAT(m, 0,0,1,0, 0,1,0,0, -1,0,0,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_perspective_fov_lh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixPerspectiveFovLH));
    D3DXMATRIX m; CHECK(F9(D3DXMatrixPerspectiveFovLH)(&m, DX_PI / 2, 1.f, 1.f, 11.f) == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1.1f,1, 0,0,-1.1f,0);
    F9(D3DXMatrixPerspectiveFovLH)(&m, DX_PI / 2, 2.f, 1.f, 11.f);
    CHECKN(m._11, 0.5f); CHECKN(m._22, 1.f);
}
TEST(d3dx_redist_optional_x9_matrix_perspective_fov_rh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixPerspectiveFovRH));
    D3DXMATRIX m; F9(D3DXMatrixPerspectiveFovRH)(&m, DX_PI / 2, 1.f, 1.f, 11.f);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,-1.1f,-1, 0,0,-1.1f,0);
}
TEST(d3dx_redist_optional_x9_matrix_perspective_lh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixPerspectiveLH));
    D3DXMATRIX m; F9(D3DXMatrixPerspectiveLH)(&m, 4.f, 2.f, 1.f, 11.f);
    CHECK_MAT(m, 0.5f,0,0,0, 0,1,0,0, 0,0,1.1f,1, 0,0,-1.1f,0);
}
TEST(d3dx_redist_optional_x9_matrix_ortho_lh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixOrthoLH));
    D3DXMATRIX m; F9(D3DXMatrixOrthoLH)(&m, 4.f, 2.f, 0.f, 1.f);
    CHECK_MAT(m, 0.5f,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    F9(D3DXMatrixOrthoLH)(&m, 2.f, 2.f, 1.f, 3.f);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,0.5f,0, 0,0,-0.5f,1);
}
TEST(d3dx_redist_optional_x9_matrix_ortho_rh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixOrthoRH));
    D3DXMATRIX m; F9(D3DXMatrixOrthoRH)(&m, 2.f, 2.f, 1.f, 3.f);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,-0.5f,0, 0,0,-0.5f,1);
}
TEST(d3dx_redist_optional_x9_matrix_ortho_offcenter_lh) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixOrthoOffCenterLH));
    D3DXMATRIX m; F9(D3DXMatrixOrthoOffCenterLH)(&m, 0.f, 4.f, 0.f, 2.f, 0.f, 1.f);
    CHECK_MAT(m, 0.5f,0,0,0, 0,1,0,0, 0,0,1,0, -1,-1,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_ortho_offcenter_symmetric_equals_ortho) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixOrthoOffCenterLH)); REQUIRE(F9(D3DXMatrixOrthoLH));
    D3DXMATRIX a, b; F9(D3DXMatrixOrthoOffCenterLH)(&a, -3.f, 3.f, -2.f, 2.f, 0.5f, 9.f); F9(D3DXMatrixOrthoLH)(&b, 6.f, 4.f, 0.5f, 9.f);
    CHECK(dx_mat_near(&a, (const float *)&b, 1e-5f));
}
TEST(d3dx_redist_optional_x9_matrix_perspective_offcenter_symmetric_equals_perspective) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixPerspectiveOffCenterLH)); REQUIRE(F9(D3DXMatrixPerspectiveLH));
    D3DXMATRIX a, b; F9(D3DXMatrixPerspectiveOffCenterLH)(&a, -2.f, 2.f, -1.f, 1.f, 1.f, 11.f); F9(D3DXMatrixPerspectiveLH)(&b, 4.f, 2.f, 1.f, 11.f);
    CHECK(dx_mat_near(&a, (const float *)&b, 1e-5f));
}
TEST(d3dx_redist_optional_x9_matrix_reflect) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixReflect));
    D3DXPLANE p = {0,1,0,0}; D3DXMATRIX m;
    CHECK(F9(D3DXMatrixReflect)(&m, &p) == &m);
    CHECK_MAT(m, 1,0,0,0, 0,-1,0,0, 0,0,1,0, 0,0,0,1);
    D3DXPLANE q = {0,1,0,-2};      /* plane y = 2 */
    F9(D3DXMatrixReflect)(&m, &q);
    CHECKN(m._22, -1.f); CHECKN(m._42, 4.f);
}
TEST(d3dx_redist_optional_x9_matrix_rotation_quaternion) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationQuaternion)); REQUIRE(F9(D3DXMatrixRotationZ));
    float s = sinf(DX_PI / 4), c = cosf(DX_PI / 4);
    D3DXQUATERNION q = {0, 0, s, c}; D3DXMATRIX m, z;
    CHECK(F9(D3DXMatrixRotationQuaternion)(&m, &q) == &m);
    F9(D3DXMatrixRotationZ)(&z, DX_PI / 2);
    CHECK(dx_mat_near(&m, (const float *)&z, 1e-5f));
}
TEST(d3dx_redist_optional_x9_matrix_yawpitchroll_single_axes) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationYawPitchRoll)); REQUIRE(F9(D3DXMatrixRotationX)); REQUIRE(F9(D3DXMatrixRotationY)); REQUIRE(F9(D3DXMatrixRotationZ));
    D3DXMATRIX a, b;
    F9(D3DXMatrixRotationYawPitchRoll)(&a, 0.8f, 0, 0); F9(D3DXMatrixRotationY)(&b, 0.8f); CHECK(dx_mat_near(&a, (const float *)&b, 1e-5f));
    F9(D3DXMatrixRotationYawPitchRoll)(&a, 0, 0.8f, 0); F9(D3DXMatrixRotationX)(&b, 0.8f); CHECK(dx_mat_near(&a, (const float *)&b, 1e-5f));
    F9(D3DXMatrixRotationYawPitchRoll)(&a, 0, 0, 0.8f); F9(D3DXMatrixRotationZ)(&b, 0.8f); CHECK(dx_mat_near(&a, (const float *)&b, 1e-5f));
}
TEST(d3dx_redist_optional_x9_matrix_yawpitchroll_order_roll_pitch_yaw) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixRotationYawPitchRoll)); REQUIRE(F9(D3DXMatrixRotationX)); REQUIRE(F9(D3DXMatrixRotationY)); REQUIRE(F9(D3DXMatrixRotationZ)); REQUIRE(F9(D3DXMatrixMultiply));
    /* documented order: roll (Z) first, then pitch (X), then yaw (Y) */
    D3DXMATRIX a, rx, ry, rz, t1, t2;
    F9(D3DXMatrixRotationYawPitchRoll)(&a, 0.5f, 0.3f, 0.9f);
    F9(D3DXMatrixRotationZ)(&rz, 0.9f); F9(D3DXMatrixRotationX)(&rx, 0.3f); F9(D3DXMatrixRotationY)(&ry, 0.5f);
    F9(D3DXMatrixMultiply)(&t1, &rz, &rx); F9(D3DXMatrixMultiply)(&t2, &t1, &ry);
    CHECK(dx_mat_near(&a, (const float *)&t2, 1e-5f));
}
TEST(d3dx_redist_optional_x9_matrix_affine_transformation_translation_scale) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixAffineTransformation));
    D3DXVECTOR3 t = {1,2,3}; D3DXMATRIX m;
    CHECK(F9(D3DXMatrixAffineTransformation)(&m, 2.f, NULL, NULL, &t) == &m);
    CHECK_MAT(m, 2,0,0,0, 0,2,0,0, 0,0,2,0, 1,2,3,1);
    F9(D3DXMatrixAffineTransformation)(&m, 1.f, NULL, NULL, NULL);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_transformation_all_null_is_identity) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTransformation));
    D3DXMATRIX m; memset(&m, 0x33, sizeof m);
    CHECK(F9(D3DXMatrixTransformation)(&m, NULL, NULL, NULL, NULL, NULL, NULL) == &m);
    CHECK_MAT(m, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_transformation_scale_translate) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTransformation));
    D3DXVECTOR3 s = {2,3,4}, t = {5,6,7}; D3DXMATRIX m;
    F9(D3DXMatrixTransformation)(&m, NULL, NULL, &s, NULL, NULL, &t);
    CHECK_MAT(m, 2,0,0,0, 0,3,0,0, 0,0,4,0, 5,6,7,1);
}
TEST(d3dx_redist_optional_x9_matrix_transformation2d) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTransformation2D));
    D3DXVECTOR2 s = {2,3}, t = {5,6}; D3DXMATRIX m;
    F9(D3DXMatrixTransformation2D)(&m, NULL, 0.f, &s, NULL, 0.f, &t);
    CHECK_MAT(m, 2,0,0,0, 0,3,0,0, 0,0,1,0, 5,6,0,1);
}
TEST(d3dx_redist_optional_x9_matrix_decompose) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixDecompose)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixRotationZ)); REQUIRE(F9(D3DXMatrixTranslation)); REQUIRE(F9(D3DXMatrixMultiply));
    D3DXMATRIX s, r, t, a, m;
    F9(D3DXMatrixScaling)(&s, 2, 3, 4); F9(D3DXMatrixRotationZ)(&r, DX_PI / 2); F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    F9(D3DXMatrixMultiply)(&a, &s, &r); F9(D3DXMatrixMultiply)(&m, &a, &t);
    D3DXVECTOR3 sc, tr; D3DXQUATERNION q;
    HRESULT hr = F9(D3DXMatrixDecompose)(&sc, &q, &tr, &m);
    EXPECT_HR(hr, S_OK);
    CHECKN(sc.x, 2.f); CHECKN(sc.y, 3.f); CHECKN(sc.z, 4.f);
    CHECKN(tr.x, 1.f); CHECKN(tr.y, 2.f); CHECKN(tr.z, 3.f);
    CHECKN(q.x, 0.f); CHECKN(q.y, 0.f); CHECKN(q.z, sinf(DX_PI / 4)); CHECKN(q.w, cosf(DX_PI / 4));
}
TEST(d3dx_redist_optional_x9_matrix_decompose_identity) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixDecompose));
    D3DXMATRIX m = dx_ident(); D3DXVECTOR3 sc, tr; D3DXQUATERNION q;
    EXPECT_HR(F9(D3DXMatrixDecompose)(&sc, &q, &tr, &m), S_OK);
    CHECKN(sc.x, 1.f); CHECKN(sc.y, 1.f); CHECKN(sc.z, 1.f);
    CHECKN(tr.x, 0.f); CHECKN(tr.y, 0.f); CHECKN(tr.z, 0.f);
    CHECKN(q.x, 0.f); CHECKN(q.y, 0.f); CHECKN(q.z, 0.f); CHECKN(q.w, 1.f);
}

/* ---- vec2 / vec3 / vec4 -------------------------------------------------- */
TEST(d3dx_redist_optional_x9_vec3_normalize) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Normalize));
    D3DXVECTOR3 v = {3,0,4}, o;
    CHECK(F9(D3DXVec3Normalize)(&o, &v) == &o);
    CHECKN(o.x, 0.6f); CHECKN(o.y, 0.f); CHECKN(o.z, 0.8f);
    D3DXVECTOR3 z = {0,0,0};
    F9(D3DXVec3Normalize)(&o, &z);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f); CHECKN(o.z, 0.f);
    F9(D3DXVec3Normalize)(&v, &v);  /* in place */
    CHECKN(v.x, 0.6f);
}
TEST(d3dx_redist_optional_x9_vec2_normalize) {
    NEED_MATH(); REQUIRE(F9(D3DXVec2Normalize));
    D3DXVECTOR2 v = {3,4}, o;
    CHECK(F9(D3DXVec2Normalize)(&o, &v) == &o);
    CHECKN(o.x, 0.6f); CHECKN(o.y, 0.8f);
    D3DXVECTOR2 z = {0,0};
    F9(D3DXVec2Normalize)(&o, &z); CHECKN(o.x, 0.f); CHECKN(o.y, 0.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_coord) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformCoord)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX t; F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXVECTOR3 v = {1,1,1}, o;
    CHECK(F9(D3DXVec3TransformCoord)(&o, &v, &t) == &o);
    CHECKN(o.x, 2.f); CHECKN(o.y, 3.f); CHECKN(o.z, 4.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_coord_divides_by_w) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformCoord)); REQUIRE(F9(D3DXMatrixPerspectiveFovLH));
    D3DXMATRIX p; F9(D3DXMatrixPerspectiveFovLH)(&p, DX_PI / 2, 1.f, 1.f, 11.f);
    D3DXVECTOR3 v = {2,4,5}, o;     /* w = z = 5 */
    F9(D3DXVec3TransformCoord)(&o, &v, &p);
    CHECKN(o.x, 0.4f); CHECKN(o.y, 0.8f); CHECKN(o.z, (1.1f * 5 - 1.1f) / 5);
}
TEST(d3dx_redist_optional_x9_vec3_transform_normal_ignores_translation) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformNormal)); REQUIRE(F9(D3DXMatrixScaling)); REQUIRE(F9(D3DXMatrixTranslation)); REQUIRE(F9(D3DXMatrixMultiply));
    D3DXMATRIX s, t, m; F9(D3DXMatrixScaling)(&s, 2, 3, 4); F9(D3DXMatrixTranslation)(&t, 9, 9, 9); F9(D3DXMatrixMultiply)(&m, &s, &t);
    D3DXVECTOR3 v = {1,1,1}, o;
    CHECK(F9(D3DXVec3TransformNormal)(&o, &v, &m) == &o);
    CHECKN(o.x, 2.f); CHECKN(o.y, 3.f); CHECKN(o.z, 4.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_vec4) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Transform)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX t; F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXVECTOR3 v = {1,1,1}; D3DXVECTOR4 o;
    CHECK(F9(D3DXVec3Transform)(&o, &v, &t) == &o);
    CHECKN(o.x, 2.f); CHECKN(o.y, 3.f); CHECKN(o.z, 4.f); CHECKN(o.w, 1.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_aliasing) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformCoord)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX t; F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXVECTOR3 v = {1,1,1};
    F9(D3DXVec3TransformCoord)(&v, &v, &t);
    CHECKN(v.x, 2.f); CHECKN(v.y, 3.f); CHECKN(v.z, 4.f);
}
TEST(d3dx_redist_optional_x9_vec2_transform_variants) {
    NEED_MATH(); REQUIRE(F9(D3DXVec2Transform)); REQUIRE(F9(D3DXVec2TransformCoord)); REQUIRE(F9(D3DXVec2TransformNormal)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX t; F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXVECTOR2 v = {5,6}, o; D3DXVECTOR4 o4;
    F9(D3DXVec2TransformCoord)(&o, &v, &t); CHECKN(o.x, 6.f); CHECKN(o.y, 8.f);
    F9(D3DXVec2TransformNormal)(&o, &v, &t); CHECKN(o.x, 5.f); CHECKN(o.y, 6.f);
    F9(D3DXVec2Transform)(&o4, &v, &t); CHECKN(o4.x, 6.f); CHECKN(o4.y, 8.f); CHECKN(o4.z, 3.f); CHECKN(o4.w, 1.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_coord_array) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformCoordArray)); REQUIRE(F9(D3DXMatrixTranslation));
    D3DXMATRIX t; F9(D3DXMatrixTranslation)(&t, 1, 2, 3);
    D3DXVECTOR3 in[3] = { {0,0,0}, {1,1,1}, {-1,-2,-3} }, out[3];
    CHECK(F9(D3DXVec3TransformCoordArray)(out, sizeof(D3DXVECTOR3), in, sizeof(D3DXVECTOR3), &t, 3) == out);
    CHECKN(out[0].x, 1.f); CHECKN(out[1].z, 4.f); CHECKN(out[2].x, 0.f); CHECKN(out[2].y, 0.f); CHECKN(out[2].z, 0.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_coord_array_strides) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformCoordArray)); REQUIRE(F9(D3DXMatrixScaling));
    D3DXMATRIX s; F9(D3DXMatrixScaling)(&s, 2, 2, 2);
    struct V { D3DXVECTOR3 p; float pad; } in[2] = { {{1,2,3}, 7}, {{4,5,6}, 8} }, out[2];
    memset(out, 0, sizeof out);
    F9(D3DXVec3TransformCoordArray)(&out[0].p, sizeof(struct V), &in[0].p, sizeof(struct V), &s, 2);
    CHECKN(out[0].p.x, 2.f); CHECKN(out[1].p.z, 12.f); CHECKN(out[0].pad, 0.f); CHECKN(out[1].pad, 0.f);
}
TEST(d3dx_redist_optional_x9_vec3_transform_normal_array) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3TransformNormalArray)); REQUIRE(F9(D3DXMatrixScaling));
    D3DXMATRIX s; F9(D3DXMatrixScaling)(&s, 1, 2, 3);
    D3DXVECTOR3 in[2] = { {1,1,1}, {0,1,0} }, out[2];
    F9(D3DXVec3TransformNormalArray)(out, sizeof(D3DXVECTOR3), in, sizeof(D3DXVECTOR3), &s, 2);
    CHECKN(out[0].z, 3.f); CHECKN(out[1].y, 2.f);
}
TEST(d3dx_redist_optional_x9_vec3_project_unproject) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Project)); REQUIRE(F9(D3DXVec3Unproject));
    D3DVIEWPORT9 vp = { 0, 0, 640, 480, 0.f, 1.f };
    D3DXMATRIX id = dx_ident();
    D3DXVECTOR3 v = {0,0,0.5f}, o;
    CHECK(F9(D3DXVec3Project)(&o, &v, &vp, &id, &id, &id) == &o);
    CHECKN(o.x, 320.f); CHECKN(o.y, 240.f); CHECKN(o.z, 0.5f);
    D3DXVECTOR3 c = {-1,1,0};
    F9(D3DXVec3Project)(&o, &c, &vp, &id, &id, &id);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f); CHECKN(o.z, 0.f);
    D3DXVECTOR3 b = {1,-1,1};
    F9(D3DXVec3Project)(&o, &b, &vp, &id, &id, &id);
    CHECKN(o.x, 640.f); CHECKN(o.y, 480.f); CHECKN(o.z, 1.f);
    D3DXVECTOR3 s = {160, 120, 0.25f}, u;
    F9(D3DXVec3Unproject)(&u, &s, &vp, &id, &id, &id);
    CHECKN(u.x, -0.5f); CHECKN(u.y, 0.5f); CHECKN(u.z, 0.25f);
}
TEST(d3dx_redist_optional_x9_vec3_project_viewport_offset_zrange) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Project));
    D3DVIEWPORT9 vp = { 10, 20, 100, 50, 0.25f, 0.75f };
    D3DXMATRIX id = dx_ident(); D3DXVECTOR3 v = {0,0,1}, o;
    F9(D3DXVec3Project)(&o, &v, &vp, &id, &id, &id);
    CHECKN(o.x, 60.f); CHECKN(o.y, 45.f); CHECKN(o.z, 0.75f);
}
TEST(d3dx_redist_optional_x9_vec3_project_unproject_roundtrip_perspective) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Project)); REQUIRE(F9(D3DXVec3Unproject)); REQUIRE(F9(D3DXMatrixPerspectiveFovLH)); REQUIRE(F9(D3DXMatrixLookAtLH));
    D3DVIEWPORT9 vp = { 0, 0, 800, 600, 0.f, 1.f };
    D3DXMATRIX p, v, w = dx_ident(); D3DXVECTOR3 eye = {1,2,-8}, at = {0,0,0}, up = {0,1,0};
    F9(D3DXMatrixPerspectiveFovLH)(&p, 0.8f, 4.f/3, 1.f, 100.f); F9(D3DXMatrixLookAtLH)(&v, &eye, &at, &up);
    D3DXVECTOR3 pt = {0.5f, -0.25f, 1.5f}, s, back;
    F9(D3DXVec3Project)(&s, &pt, &vp, &p, &v, &w);
    F9(D3DXVec3Unproject)(&back, &s, &vp, &p, &v, &w);
    CHECKNE(back.x, pt.x, 1e-3f); CHECKNE(back.y, pt.y, 1e-3f); CHECKNE(back.z, pt.z, 1e-3f);
}
TEST(d3dx_redist_optional_x9_vec3_hermite_endpoints) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Hermite));
    D3DXVECTOR3 p1 = {0,0,0}, t1 = {1,0,0}, p2 = {4,2,0}, t2 = {0,1,0}, o;
    CHECK(F9(D3DXVec3Hermite)(&o, &p1, &t1, &p2, &t2, 0.f) == &o);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f);
    F9(D3DXVec3Hermite)(&o, &p1, &t1, &p2, &t2, 1.f);
    CHECKN(o.x, 4.f); CHECKN(o.y, 2.f);
    F9(D3DXVec3Hermite)(&o, &p1, &t1, &p2, &t2, 0.5f);  /* h00=h01=.5, h10=.125, h11=-.125 */
    CHECKN(o.x, 2.125f); CHECKN(o.y, 1.f - 0.125f);
}
TEST(d3dx_redist_optional_x9_vec3_catmullrom_endpoints) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3CatmullRom));
    D3DXVECTOR3 a = {-1,0,0}, b = {0,0,0}, c = {1,1,0}, d = {5,5,0}, o;
    CHECK(F9(D3DXVec3CatmullRom)(&o, &a, &b, &c, &d, 0.f) == &o);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f);
    F9(D3DXVec3CatmullRom)(&o, &a, &b, &c, &d, 1.f);
    CHECKN(o.x, 1.f); CHECKN(o.y, 1.f);
}
TEST(d3dx_redist_optional_x9_vec3_baryentric) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3BaryCentric));
    D3DXVECTOR3 a = {0,0,0}, b = {2,0,0}, c = {0,4,0}, o;
    CHECK(F9(D3DXVec3BaryCentric)(&o, &a, &b, &c, 0.5f, 0.25f) == &o);
    CHECKN(o.x, 1.f); CHECKN(o.y, 1.f); CHECKN(o.z, 0.f);
    F9(D3DXVec3BaryCentric)(&o, &a, &b, &c, 0.f, 0.f); CHECKN(o.x, 0.f);
    F9(D3DXVec3BaryCentric)(&o, &a, &b, &c, 1.f, 0.f); CHECKN(o.x, 2.f);
    F9(D3DXVec3BaryCentric)(&o, &a, &b, &c, 0.f, 1.f); CHECKN(o.y, 4.f);
}
TEST(d3dx_redist_optional_x9_vec2_baryentric_hermite_catmull) {
    NEED_MATH(); REQUIRE(F9(D3DXVec2BaryCentric)); REQUIRE(F9(D3DXVec2Hermite)); REQUIRE(F9(D3DXVec2CatmullRom));
    D3DXVECTOR2 a = {0,0}, b = {2,0}, c = {0,4}, o;
    F9(D3DXVec2BaryCentric)(&o, &a, &b, &c, 0.5f, 0.5f); CHECKN(o.x, 1.f); CHECKN(o.y, 2.f);
    F9(D3DXVec2Hermite)(&o, &a, &b, &c, &a, 0.f); CHECKN(o.x, 0.f);
    F9(D3DXVec2Hermite)(&o, &a, &b, &c, &a, 1.f); CHECKN(o.x, 0.f); CHECKN(o.y, 4.f);
    D3DXVECTOR2 d = {7,7};
    F9(D3DXVec2CatmullRom)(&o, &a, &b, &c, &d, 0.f); CHECKN(o.x, 2.f); CHECKN(o.y, 0.f);
    F9(D3DXVec2CatmullRom)(&o, &a, &b, &c, &d, 1.f); CHECKN(o.x, 0.f); CHECKN(o.y, 4.f);
}

/* ---- planes ------------------------------------------------------------- */
TEST(d3dx_redist_optional_x9_plane_from_point_normal) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneFromPointNormal));
    D3DXVECTOR3 p = {0,3,0}, n = {0,1,0}; D3DXPLANE o;
    CHECK(F9(D3DXPlaneFromPointNormal)(&o, &p, &n) == &o);
    CHECKN(o.a, 0.f); CHECKN(o.b, 1.f); CHECKN(o.c, 0.f); CHECKN(o.d, -3.f);
    D3DXVECTOR3 n2 = {0,2,0};   /* the normal is used as given, not normalised */
    F9(D3DXPlaneFromPointNormal)(&o, &p, &n2);
    CHECKN(o.b, 2.f); CHECKN(o.d, -6.f);
}
TEST(d3dx_redist_optional_x9_plane_from_points) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneFromPoints));
    D3DXVECTOR3 a = {0,0,0}, b = {1,0,0}, c = {0,1,0}; D3DXPLANE o;
    CHECK(F9(D3DXPlaneFromPoints)(&o, &a, &b, &c) == &o);
    CHECKN(o.a, 0.f); CHECKN(o.b, 0.f); CHECKN(o.c, 1.f); CHECKN(o.d, 0.f);
    D3DXVECTOR3 a2 = {0,0,2}, b2 = {5,0,2}, c2 = {0,3,2};
    F9(D3DXPlaneFromPoints)(&o, &a2, &b2, &c2);
    CHECKN(o.c, 1.f); CHECKN(o.d, -2.f);
    F9(D3DXPlaneFromPoints)(&o, &a2, &c2, &b2);   /* reversed winding flips the normal */
    CHECKN(o.c, -1.f); CHECKN(o.d, 2.f);
}
TEST(d3dx_redist_optional_x9_plane_normalize) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneNormalize));
    D3DXPLANE p = {0,0,2,4}, o;
    CHECK(F9(D3DXPlaneNormalize)(&o, &p) == &o);
    CHECKN(o.a, 0.f); CHECKN(o.c, 1.f); CHECKN(o.d, 2.f);
    D3DXPLANE q = {3,0,4,10};
    F9(D3DXPlaneNormalize)(&q, &q); CHECKN(q.a, 0.6f); CHECKN(q.c, 0.8f); CHECKN(q.d, 2.f);
    D3DXPLANE z = {0,0,0,5};
    F9(D3DXPlaneNormalize)(&o, &z); CHECKN(o.a, 0.f); CHECKN(o.b, 0.f); CHECKN(o.c, 0.f);
    CHECK(isinf(o.d) && o.d > 0);   /* d / |n| with |n| == 0: +inf, no special case (probed) */
    D3DXPLANE z0 = {0,0,0,0};
    F9(D3DXPlaneNormalize)(&o, &z0); CHECKN(o.a, 0.f); CHECKN(o.b, 0.f); CHECKN(o.c, 0.f); CHECKN(o.d, 0.f);
}
TEST(d3dx_redist_optional_x9_plane_intersect_line) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneIntersectLine));
    D3DXPLANE p = {0,0,1,-1}; D3DXVECTOR3 a = {0,0,0}, b = {0,0,2}, o;
    CHECK(F9(D3DXPlaneIntersectLine)(&o, &p, &a, &b) == &o);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f); CHECKN(o.z, 1.f);
    D3DXVECTOR3 c = {3,4,5}, d = {7,8,-5};
    F9(D3DXPlaneIntersectLine)(&o, &p, &c, &d);
    CHECKN(o.z, 1.f); CHECKN(o.x, 3.f + 4 * 0.4f);
}
TEST(d3dx_redist_optional_x9_plane_intersect_line_parallel) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneIntersectLine));
    D3DXPLANE p = {0,0,1,-1}; D3DXVECTOR3 a = {0,0,3}, b = {5,5,3}, o = {9,9,9};
    CHECK(F9(D3DXPlaneIntersectLine)(&o, &p, &a, &b) == NULL);
}
TEST(d3dx_redist_optional_x9_plane_transform_identity) {
    NEED_MATH(); REQUIRE(F9(D3DXPlaneTransform));
    D3DXPLANE p = {1,2,3,4}, o; D3DXMATRIX id = dx_ident();
    CHECK(F9(D3DXPlaneTransform)(&o, &p, &id) == &o);
    CHECKN(o.a, 1.f); CHECKN(o.b, 2.f); CHECKN(o.c, 3.f); CHECKN(o.d, 4.f);
}

/* ---- quaternions -------------------------------------------------------- */
TEST(d3dx_redist_optional_x9_quaternion_rotation_axis) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 ax = {0,0,1}; D3DXQUATERNION q;
    CHECK(F9(D3DXQuaternionRotationAxis)(&q, &ax, DX_PI / 2) == &q);
    CHECKN(q.x, 0.f); CHECKN(q.y, 0.f); CHECKN(q.z, sinf(DX_PI / 4)); CHECKN(q.w, cosf(DX_PI / 4));
    D3DXVECTOR3 ax2 = {0,5,0};   /* axis is normalised by the function */
    F9(D3DXQuaternionRotationAxis)(&q, &ax2, DX_PI);
    CHECKN(q.y, 1.f); CHECKN(q.w, 0.f);
    F9(D3DXQuaternionRotationAxis)(&q, &ax, 0.f);
    CHECKN(q.z, 0.f); CHECKN(q.w, 1.f);
}
TEST(d3dx_redist_optional_x9_quaternion_multiply) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionMultiply)); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 z = {0,0,1}; D3DXQUATERNION a, b, o;
    F9(D3DXQuaternionRotationAxis)(&a, &z, DX_PI / 2); b = a;
    CHECK(F9(D3DXQuaternionMultiply)(&o, &a, &b) == &o);   /* two 90 degree turns = 180 */
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f); CHECKN(o.z, 1.f); CHECKN(o.w, 0.f);
    D3DXQUATERNION i = {0,0,0,1};
    F9(D3DXQuaternionMultiply)(&o, &a, &i); CHECKN(o.z, a.z); CHECKN(o.w, a.w);
    F9(D3DXQuaternionMultiply)(&a, &a, &a);   /* aliasing */
    CHECKN(a.z, 1.f); CHECKN(a.w, 0.f);
}
TEST(d3dx_redist_optional_x9_quaternion_multiply_noncommutative) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionMultiply));
    D3DXQUATERNION i = {1,0,0,0}, j = {0,1,0,0}, ij, ji;
    F9(D3DXQuaternionMultiply)(&ij, &i, &j); F9(D3DXQuaternionMultiply)(&ji, &j, &i);
    /* D3DX multiply(a,b) composes 'a followed by b' (Hamilton b*a): multiply(i,j) = -k, multiply(j,i) = +k */
    CHECKN(ij.z, -1.f); CHECKN(ji.z, 1.f);
    CHECKN(ij.w, 0.f);
}
TEST(d3dx_redist_optional_x9_quaternion_normalize) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionNormalize));
    D3DXQUATERNION q = {0,0,3,4}, o;
    CHECK(F9(D3DXQuaternionNormalize)(&o, &q) == &o);
    CHECKN(o.z, 0.6f); CHECKN(o.w, 0.8f);
    D3DXQUATERNION z = {0,0,0,0};
    F9(D3DXQuaternionNormalize)(&o, &z); t_note("normalize zero quaternion -> (%g %g %g %g)", o.x, o.y, o.z, o.w);
}
TEST(d3dx_redist_optional_x9_quaternion_inverse) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionInverse)); REQUIRE(F9(D3DXQuaternionMultiply)); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 ax = {1,1,0}; D3DXQUATERNION q, inv, p;
    F9(D3DXQuaternionRotationAxis)(&q, &ax, 1.1f);
    CHECK(F9(D3DXQuaternionInverse)(&inv, &q) == &inv);
    F9(D3DXQuaternionMultiply)(&p, &q, &inv);
    CHECKN(p.x, 0.f); CHECKN(p.y, 0.f); CHECKN(p.z, 0.f); CHECKN(p.w, 1.f);
    D3DXQUATERNION nz = {0,0,2,0};   /* non unit: inverse = conj / |q|^2 */
    F9(D3DXQuaternionInverse)(&inv, &nz); CHECKN(inv.z, -0.5f);
}
TEST(d3dx_redist_optional_x9_quaternion_yawpitchroll) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionRotationYawPitchRoll));
    D3DXQUATERNION q;
    F9(D3DXQuaternionRotationYawPitchRoll)(&q, DX_PI / 2, 0, 0);
    CHECKN(q.x, 0.f); CHECKN(q.y, sinf(DX_PI / 4)); CHECKN(q.z, 0.f); CHECKN(q.w, cosf(DX_PI / 4));
    F9(D3DXQuaternionRotationYawPitchRoll)(&q, 0, DX_PI / 2, 0);
    CHECKN(q.x, sinf(DX_PI / 4)); CHECKN(q.y, 0.f);
    F9(D3DXQuaternionRotationYawPitchRoll)(&q, 0, 0, DX_PI / 2);
    CHECKN(q.z, sinf(DX_PI / 4));
}
TEST(d3dx_redist_optional_x9_quaternion_slerp) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionSlerp)); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 z = {0,0,1}; D3DXQUATERNION a = {0,0,0,1}, b, o;
    F9(D3DXQuaternionRotationAxis)(&b, &z, DX_PI / 2);
    CHECK(F9(D3DXQuaternionSlerp)(&o, &a, &b, 0.5f) == &o);
    CHECKN(o.z, sinf(DX_PI / 8)); CHECKN(o.w, cosf(DX_PI / 8));
    F9(D3DXQuaternionSlerp)(&o, &a, &b, 0.f); CHECKN(o.z, 0.f); CHECKN(o.w, 1.f);
    F9(D3DXQuaternionSlerp)(&o, &a, &b, 1.f); CHECKN(o.z, b.z); CHECKN(o.w, b.w);
}
TEST(d3dx_redist_optional_x9_quaternion_slerp_shortest_path) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionSlerp));
    D3DXQUATERNION a = {0,0,0,1}, b = {0,0,0,-1}, o;   /* same rotation, opposite hemisphere */
    F9(D3DXQuaternionSlerp)(&o, &a, &b, 0.5f);
    CHECKN(fabsf(o.w), 1.f); CHECKN(o.z, 0.f);
}
TEST(d3dx_redist_optional_x9_quaternion_to_axis_angle) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionToAxisAngle)); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 ax = {0,1,0}, ro; float angle = -1;
    D3DXQUATERNION q; F9(D3DXQuaternionRotationAxis)(&q, &ax, 1.2f);
    F9(D3DXQuaternionToAxisAngle)(&q, &ro, &angle);
    CHECKN(ro.x, 0.f); CHECKN(ro.z, 0.f); CHECK(ro.y > 0.f); CHECKN(angle, 1.2f);
    t_note("axis returned has length %g", sqrtf(ro.x*ro.x + ro.y*ro.y + ro.z*ro.z));
}
TEST(d3dx_redist_optional_x9_quaternion_rotation_matrix_roundtrip) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionRotationMatrix)); REQUIRE(F9(D3DXMatrixRotationQuaternion)); REQUIRE(F9(D3DXQuaternionRotationAxis));
    D3DXVECTOR3 ax = {1,2,3}; D3DXQUATERNION q, q2; D3DXMATRIX m;
    F9(D3DXQuaternionRotationAxis)(&q, &ax, 0.7f);
    F9(D3DXMatrixRotationQuaternion)(&m, &q);
    CHECK(F9(D3DXQuaternionRotationMatrix)(&q2, &m) == &q2);
    float sgn = (q.w * q2.w + q.x * q2.x + q.y * q2.y + q.z * q2.z) < 0 ? -1.f : 1.f;
    CHECKNE(q2.x * sgn, q.x, 1e-5f); CHECKNE(q2.y * sgn, q.y, 1e-5f); CHECKNE(q2.z * sgn, q.z, 1e-5f); CHECKNE(q2.w * sgn, q.w, 1e-5f);
}
TEST(d3dx_redist_optional_x9_quaternion_exp_ln) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionExp)); REQUIRE(F9(D3DXQuaternionLn));
    D3DXQUATERNION id = {0,0,0,1}, o;
    CHECK(F9(D3DXQuaternionLn)(&o, &id) == &o);
    CHECKN(o.x, 0.f); CHECKN(o.y, 0.f); CHECKN(o.z, 0.f); CHECKN(o.w, 0.f);
    D3DXQUATERNION z = {0,0,0,0};
    CHECK(F9(D3DXQuaternionExp)(&o, &z) == &o);
    CHECKN(o.w, 1.f); CHECKN(o.z, 0.f);
    D3DXQUATERNION q = {0,0,sinf(0.4f),cosf(0.4f)}, l, e;
    F9(D3DXQuaternionLn)(&l, &q); CHECKN(l.z, 0.4f); CHECKN(l.w, 0.f);
    F9(D3DXQuaternionExp)(&e, &l); CHECKN(e.z, q.z); CHECKN(e.w, q.w);
}
TEST(d3dx_redist_optional_x9_quaternion_baryentric_corners) {
    NEED_MATH(); REQUIRE(F9(D3DXQuaternionBaryCentric));
    D3DXQUATERNION a = {0,0,0,1}, b = {0,0,sinf(0.1f),cosf(0.1f)}, c = {0,0,sinf(0.2f),cosf(0.2f)}, o;
    CHECK(F9(D3DXQuaternionBaryCentric)(&o, &a, &b, &c, 1.f, 0.f) == &o);
    CHECKNE(o.z, b.z, 1e-5f); CHECKNE(o.w, b.w, 1e-5f);
    F9(D3DXQuaternionBaryCentric)(&o, &a, &b, &c, 0.f, 1.f); CHECKNE(o.z, c.z, 1e-5f); CHECKNE(o.w, c.w, 1e-5f);
}

/* ---- colors / fresnel --------------------------------------------------- */
TEST(d3dx_redist_optional_x9_color_adjust_contrast) {
    NEED_MATH(); REQUIRE(F9(D3DXColorAdjustContrast));
    D3DXCOLOR c = {0.25f, 0.5f, 1.f, 0.3f}, o;
    CHECK(F9(D3DXColorAdjustContrast)(&o, &c, 2.f) == &o);
    CHECKN(o.r, 0.f); CHECKN(o.g, 0.5f); CHECKN(o.b, 1.5f); CHECKN(o.a, 0.3f);
    F9(D3DXColorAdjustContrast)(&o, &c, 0.f);
    CHECKN(o.r, 0.5f); CHECKN(o.g, 0.5f); CHECKN(o.b, 0.5f); CHECKN(o.a, 0.3f);
    F9(D3DXColorAdjustContrast)(&o, &c, 1.f);
    CHECKN(o.r, 0.25f); CHECKN(o.b, 1.f);
}
TEST(d3dx_redist_optional_x9_color_adjust_saturation) {
    NEED_MATH(); REQUIRE(F9(D3DXColorAdjustSaturation));
    D3DXCOLOR c = {1.f, 0.f, 0.f, 0.4f}, o;
    CHECK(F9(D3DXColorAdjustSaturation)(&o, &c, 1.f) == &o);
    CHECKN(o.r, 1.f); CHECKN(o.g, 0.f); CHECKN(o.a, 0.4f);
    F9(D3DXColorAdjustSaturation)(&o, &c, 0.f);
    CHECKN(o.r, o.g); CHECKN(o.g, o.b); CHECKNE(o.r, 0.2125f, 1e-3f); CHECKN(o.a, 0.4f);
}
TEST(d3dx_redist_optional_x9_fresnel_term) {
    NEED_MATH(); REQUIRE(F9(D3DXFresnelTerm));
    /* normal incidence: ((n-1)/(n+1))^2 */
    CHECKNE(F9(D3DXFresnelTerm)(1.f, 1.5f), 0.04f, 1e-5f);
    CHECKNE(F9(D3DXFresnelTerm)(1.f, 2.f), 1.f / 9.f, 1e-5f);
    CHECKNE(F9(D3DXFresnelTerm)(1.f, 1.f), 0.f, 1e-6f);
    float prev = 1e9f, f;   /* reflectance falls monotonically as the angle gets closer to the normal */
    for (int i = 1; i <= 10; i++) { f = F9(D3DXFresnelTerm)(i / 10.f, 1.5f); CHECK(f <= prev + 1e-6f); prev = f; }
}

/* ---- float16 ------------------------------------------------------------ */
TEST(d3dx_redist_optional_x9_float16_to_32_known) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat16To32Array));
    static const WORD in[] = { 0x3C00, 0xC000, 0x3800, 0x0000, 0x7BFF, 0x0400, 0x0001, 0x7C00, 0xFC00, 0x4900 };
    static const float want[] = { 1.f, -2.f, 0.5f, 0.f, 65504.f, 6.103515625e-5f, 5.9604644775390625e-8f, INFINITY, -INFINITY, 10.0f };
    float out[10];
    FLOAT *r = F9(D3DXFloat16To32Array)(out, (const D3DXFLOAT16 *)in, 10);
    CHECK(r == out);
    for (int i = 0; i < 10; i++) {
        if (isinf(want[i]) || in[i] == 0x0001) { t_note("half 0x%04x (inf/denormal) -> %g", in[i], out[i]); continue; }
        CHECKF(out[i] == want[i], "idx %d got %.9g want %.9g", i, out[i], want[i]);
    }
}
TEST(d3dx_redist_optional_x9_float16_to_32_nan) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat16To32Array));
    WORD in[2] = { 0x7E00, 0x7C01 }; float out[2];
    F9(D3DXFloat16To32Array)(out, (const D3DXFLOAT16 *)in, 2);
    t_note("half NaN patterns 0x7e00/0x7c01 -> %g / %g", out[0], out[1]);
}
TEST(d3dx_redist_optional_x9_float16_to_32_negative_zero) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat16To32Array));
    WORD in = 0x8000; float out = 1;
    F9(D3DXFloat16To32Array)(&out, (const D3DXFLOAT16 *)&in, 1);
    CHECK(out == 0.f); t_note("half -0 sign preserved: %d", signbit(out) != 0);
}
TEST(d3dx_redist_optional_x9_float32_to_16_known) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat32To16Array));
    static const float in[] = { 1.f, -2.f, 0.5f, 0.f, 65504.f, INFINITY, -INFINITY, 10.f, 0.25f, 3.f, -0.f };
    static const WORD want[] = { 0x3C00, 0xC000, 0x3800, 0x0000, 0x7BFF, 0x7C00, 0xFC00, 0x4900, 0x3400, 0x4200, 0x8000 };
    WORD out[11];
    D3DXFLOAT16 *r = F9(D3DXFloat32To16Array)((D3DXFLOAT16 *)out, in, 11);
    CHECK(r == (D3DXFLOAT16 *)out);
    for (int i = 0; i < 11; i++) {
        if (isinf(in[i]) || (in[i] == 0.f && signbit(in[i]))) { t_note("float %g -> half 0x%04x", in[i], out[i]); continue; }
        CHECKF(out[i] == want[i], "idx %d got 0x%04x want 0x%04x", i, out[i], want[i]);
    }
}
TEST(d3dx_redist_optional_x9_float32_to_16_count_zero_writes_nothing) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat32To16Array)); REQUIRE(F9(D3DXFloat16To32Array));
    WORD out = 0xABCD; float f = 1.f;
    F9(D3DXFloat32To16Array)((D3DXFLOAT16 *)&out, &f, 0); CHECK(out == 0xABCD);
    float o = 7.f; WORD w = 0x3C00;
    F9(D3DXFloat16To32Array)(&o, (const D3DXFLOAT16 *)&w, 0); CHECK(o == 7.f);
}
TEST(d3dx_redist_optional_x9_float16_roundtrip_all_normals) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat32To16Array)); REQUIRE(F9(D3DXFloat16To32Array));
    static WORD h[65536], back[65536]; static float f[65536];
    for (int i = 0; i < 65536; i++) h[i] = (WORD)i;
    F9(D3DXFloat16To32Array)(f, (const D3DXFLOAT16 *)h, 65536);
    F9(D3DXFloat32To16Array)((D3DXFLOAT16 *)back, f, 65536);
    int badn = 0, badd = 0;
    for (int i = 0; i < 65536; i++) {
        unsigned e = (i >> 10) & 31, m = i & 1023;
        if (e == 31 && m) continue;           /* NaNs */
        if (back[i] == h[i]) continue;
        if (e == 0) badd++; else badn++;
    }
    CHECKF(badn == 0, "%d normal/inf values do not roundtrip", badn);
    if (badd) t_note("float16 denormals not roundtripping: %d", badd);
}
TEST(d3dx_redist_optional_x9_float32_to_16_rounding_and_overflow) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat32To16Array));
    float in[] = { 1.0009765625f /* 1+2^-10 exact */, 1e10f, -1e10f };
    WORD out[3]; F9(D3DXFloat32To16Array)((D3DXFLOAT16 *)out, in, 3);
    CHECKF(out[0] == 0x3C01, "got 0x%04x", out[0]);
    t_note("float32->16 overflow of 1e10 gives 0x%04x / 0x%04x (inf=0x7c00/0xfc00)", out[1], out[2]);
}

/* ---- spherical harmonics ------------------------------------------------ */
TEST(d3dx_redist_optional_x9_sh_eval_direction_z_order3) {
    NEED_MATH(); REQUIRE(F9(D3DXSHEvalDirection));
    D3DXVECTOR3 d = {0,0,1}; float o[9]; for (int i = 0; i < 9; i++) o[i] = -77.f;
    FLOAT *r = F9(D3DXSHEvalDirection)(o, 3, &d);
    CHECK(r == o);
    CHECKNE(o[0], 0.282094792f, 1e-6f);
    CHECKNE(o[1], 0.f, 1e-6f); CHECKNE(o[2], 0.488602512f, 1e-6f); CHECKNE(o[3], 0.f, 1e-6f);
    CHECKNE(o[4], 0.f, 1e-6f); CHECKNE(o[5], 0.f, 1e-6f); CHECKNE(o[6], 0.630783131f, 1e-6f);
    CHECKNE(o[7], 0.f, 1e-6f); CHECKNE(o[8], 0.f, 1e-6f);
}
TEST(d3dx_redist_optional_x9_sh_eval_direction_axes_order2) {
    NEED_MATH(); REQUIRE(F9(D3DXSHEvalDirection));
    float o[4];
    D3DXVECTOR3 x = {1,0,0}, y = {0,1,0}, z = {0,0,1};
    F9(D3DXSHEvalDirection)(o, 2, &x); CHECKNE(o[0], 0.282094792f, 1e-6f); CHECKNE(fabsf(o[3]), 0.488602512f, 1e-6f); CHECKNE(o[1], 0.f, 1e-6f); CHECKNE(o[2], 0.f, 1e-6f);
    F9(D3DXSHEvalDirection)(o, 2, &y); CHECKNE(fabsf(o[1]), 0.488602512f, 1e-6f); CHECKNE(o[2], 0.f, 1e-6f); CHECKNE(o[3], 0.f, 1e-6f);
    F9(D3DXSHEvalDirection)(o, 2, &z); CHECKNE(o[2], 0.488602512f, 1e-6f);
    D3DXVECTOR3 mz = {0,0,-1};
    F9(D3DXSHEvalDirection)(o, 2, &mz); CHECKNE(o[2], -0.488602512f, 1e-6f);
}
TEST(d3dx_redist_optional_x9_sh_eval_direction_sum_of_squares) {
    NEED_MATH(); REQUIRE(F9(D3DXSHEvalDirection));
    /* addition theorem: per band l, sum_m Y_lm(d)^2 = (2l+1)/(4 pi) for unit d */
    D3DXVECTOR3 d = {0.36f, 0.48f, 0.8f}; float o[25];
    F9(D3DXSHEvalDirection)(o, 5, &d);
    for (int l = 0; l < 5; l++) {
        float s = 0; for (int m = 0; m < 2 * l + 1; m++) s += o[l * l + m] * o[l * l + m];
        CHECKF(dx_near(s, (2 * l + 1) / (4 * DX_PI), 1e-4f), "band %d sum %.7g", l, s);
    }
}
TEST(d3dx_redist_optional_x9_sh_eval_direction_order_range) {
    NEED_MATH(); REQUIRE(F9(D3DXSHEvalDirection));
    D3DXVECTOR3 d = {0,0,1}; float o[36];
    for (UINT ord = 2; ord <= 6; ord++) { memset(o, 0, sizeof o); CHECK(F9(D3DXSHEvalDirection)(o, ord, &d) == o); CHECKNE(o[0], 0.282094792f, 1e-6f); }
    memset(o, 0, sizeof o);
    FLOAT *r1 = F9(D3DXSHEvalDirection)(o, 1, &d);
    FLOAT *r7 = F9(D3DXSHEvalDirection)(o, 7, &d);
    t_note("SHEvalDirection order 1 -> %p, order 7 -> %p (out=%p)", (void *)r1, (void *)r7, (void *)o);
}
TEST(d3dx_redist_optional_x9_sh_add_scale_dot) {
    NEED_MATH(); REQUIRE(F9(D3DXSHAdd)); REQUIRE(F9(D3DXSHScale)); REQUIRE(F9(D3DXSHDot));
    float a[9], b[9], o[9];
    for (int i = 0; i < 9; i++) { a[i] = (float)(i + 1); b[i] = 2.f * i - 3.f; }
    CHECK(F9(D3DXSHAdd)(o, 3, a, b) == o);
    for (int i = 0; i < 9; i++) CHECKN(o[i], a[i] + b[i]);
    CHECK(F9(D3DXSHScale)(o, 3, a, 2.5f) == o);
    for (int i = 0; i < 9; i++) CHECKN(o[i], a[i] * 2.5f);
    float ref = 0; for (int i = 0; i < 9; i++) ref += a[i] * b[i];
    CHECKN(F9(D3DXSHDot)(3, a, b), ref);
    float ref2 = 0; for (int i = 0; i < 4; i++) ref2 += a[i] * b[i];
    CHECKN(F9(D3DXSHDot)(2, a, b), ref2);
}
TEST(d3dx_redist_optional_x9_sh_add_in_place) {
    NEED_MATH(); REQUIRE(F9(D3DXSHAdd));
    float a[4] = {1,2,3,4}, b[4] = {10,20,30,40};
    F9(D3DXSHAdd)(a, 2, a, b);
    CHECKN(a[0], 11.f); CHECKN(a[3], 44.f);
}
TEST(d3dx_redist_optional_x9_sh_multiply2_dc) {
    NEED_MATH(); REQUIRE(F9(D3DXSHMultiply2));
    /* constant function times constant function: (a0 Y00)(b0 Y00) = a0 b0 Y00 * Y00 */
    float a[4] = {2,0,0,0}, b[4] = {3,0,0,0}, o[4];
    F9(D3DXSHMultiply2)(o, a, b);
    CHECKNE(o[0], 6.f * 0.282094792f, 1e-5f); CHECKNE(o[1], 0.f, 1e-6f); CHECKNE(o[2], 0.f, 1e-6f); CHECKNE(o[3], 0.f, 1e-6f);
}
TEST(d3dx_redist_optional_x9_sh_multiply2_by_one) {
    NEED_MATH(); REQUIRE(F9(D3DXSHMultiply2));
    /* multiplying by the constant function 1 (= sqrt(4 pi) Y00) is the identity */
    float one[4] = {3.5449077018f, 0, 0, 0}, b[4] = {0.5f, -0.25f, 0.75f, 1.f}, o[4];
    F9(D3DXSHMultiply2)(o, one, b);
    for (int i = 0; i < 4; i++) CHECKNE(o[i], b[i], 1e-4f);
}
TEST(d3dx_redist_optional_x9_sh_rotate_z_invariants) {
    NEED_MATH(); REQUIRE(F9(D3DXSHRotateZ));
    float in[9] = {1, .5f, -.25f, .75f, 2, -1, .3f, .9f, -.6f}, o[9], o2[9];
    CHECK(F9(D3DXSHRotateZ)(o, 3, 0.f, in) == o);
    for (int i = 0; i < 9; i++) CHECKNE(o[i], in[i], 1e-6f);
    F9(D3DXSHRotateZ)(o, 3, 0.7f, in);
    CHECKNE(o[0], in[0], 1e-6f); CHECKNE(o[2], in[2], 1e-6f); CHECKNE(o[6], in[6], 1e-6f);  /* m == 0 terms */
    float n1 = 0, n2 = 0; for (int i = 0; i < 9; i++) { n1 += in[i] * in[i]; n2 += o[i] * o[i]; }
    CHECKNE(n2, n1, 1e-4f);
    F9(D3DXSHRotateZ)(o2, 3, 2 * DX_PI, in);
    for (int i = 0; i < 9; i++) CHECKNE(o2[i], in[i], 1e-4f);
}
TEST(d3dx_redist_optional_x9_sh_rotate_z_composition) {
    NEED_MATH(); REQUIRE(F9(D3DXSHRotateZ));
    float in[9] = {1, .5f, -.25f, .75f, 2, -1, .3f, .9f, -.6f}, a[9], b[9], c[9];
    F9(D3DXSHRotateZ)(a, 3, 0.4f, in); F9(D3DXSHRotateZ)(b, 3, 0.5f, a); F9(D3DXSHRotateZ)(c, 3, 0.9f, in);
    for (int i = 0; i < 9; i++) CHECKNE(b[i], c[i], 1e-4f);
}
TEST(d3dx_redist_optional_x9_sh_rotate_identity_matrix_order3) {
    NEED_MATH(); REQUIRE(F9(D3DXSHRotate));
    D3DXMATRIX id = dx_ident(); float in[9], o[9];
    for (int i = 0; i < 9; i++) in[i] = 0.1f * (i + 1);
    CHECK(F9(D3DXSHRotate)(o, 3, &id, in) == o);
    for (int i = 0; i < 9; i++) CHECKNE(o[i], in[i], 1e-5f);
}
TEST(d3dx_redist_optional_x9_sh_rotate_preserves_norm) {
    NEED_MATH(); REQUIRE(F9(D3DXSHRotate)); REQUIRE(F9(D3DXMatrixRotationYawPitchRoll));
    D3DXMATRIX r; F9(D3DXMatrixRotationYawPitchRoll)(&r, 0.3f, 0.5f, -0.8f);
    float in[16], o[16];
    for (int i = 0; i < 16; i++) in[i] = 0.2f * (float)((i * 7) % 5) - 0.3f;
    F9(D3DXSHRotate)(o, 4, &r, in);
    for (int l = 0; l < 4; l++) {   /* each band is rotated independently, band energy is invariant */
        float e1 = 0, e2 = 0; for (int m = 0; m < 2 * l + 1; m++) { e1 += in[l*l+m] * in[l*l+m]; e2 += o[l*l+m] * o[l*l+m]; }
        CHECKF(dx_near(e1, e2, 1e-4f), "band %d energy %g vs %g", l, e1, e2);
    }
}

/* ---- export presence table ----------------------------------------------- */
TEST(d3dx_redist_optional_x9_math_exports_presence) {
    NEED_MATH();
    static const char *names[] = { "D3DXMatrixIdentity","D3DXMatrixMultiply","D3DXMatrixInverse","D3DXMatrixDeterminant","D3DXMatrixTranspose","D3DXMatrixLookAtLH","D3DXMatrixPerspectiveFovLH",
        "D3DXVec3Normalize","D3DXVec3TransformCoord","D3DXQuaternionSlerp","D3DXPlaneNormalize","D3DXSHEvalDirection","D3DXFloat16To32Array","D3DXCheckVersion",
        "D3DXVec3Cross","D3DXVec3Dot","D3DXVec3Length","D3DXVec3Lerp","D3DXMatrixIsIdentity","D3DXQuaternionDot","D3DXPlaneDotCoord" };
    HMODULE m = LoadLibraryA(X9);
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++)
        t_note("%s %s", names[i], GetProcAddress(m, names[i]) ? "exported" : "not exported (inline in headers)");
}

/* ---- additional invariants ---------------------------------------------- */
TEST(d3dx_redist_optional_x9_float16_roundtrip_exact) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat16To32Array)); REQUIRE(F9(D3DXFloat32To16Array));
    static const float in[] = { 1.f, -1.f, 2.f, 0.125f, 1024.f, -3.5f, 100.f, 0.0009765625f };
    WORD h[8]; float back[8];
    F9(D3DXFloat32To16Array)((D3DXFLOAT16 *)h, in, 8);
    F9(D3DXFloat16To32Array)(back, (const D3DXFLOAT16 *)h, 8);
    for (int i = 0; i < 8; i++) CHECKF(back[i] == in[i], "idx %d got %g want %g", i, back[i], in[i]);
}
TEST(d3dx_redist_optional_x9_float16_zero_count) {
    NEED_MATH(); REQUIRE(F9(D3DXFloat16To32Array));
    float out[2] = { 7.f, 8.f }; WORD in[2] = { 0x3C00, 0x3C00 };
    F9(D3DXFloat16To32Array)(out, (const D3DXFLOAT16 *)in, 0);
    CHECK(out[0] == 7.f && out[1] == 8.f);
}
TEST(d3dx_redist_optional_x9_matrix_transpose_involution) {
    NEED_MATH(); REQUIRE(F9(D3DXMatrixTranspose));
    D3DXMATRIX m, t, u; float *f = (float *)&m; for (int i = 0; i < 16; i++) f[i] = (float)(i + 1);
    CHECK(F9(D3DXMatrixTranspose)(&t, &m) == &t);
    CHECKN(t._12, m._21); CHECKN(t._41, m._14); CHECKN(t._33, m._33);
    F9(D3DXMatrixTranspose)(&u, &t);
    CHECK(dx_mat_near(&u, (const float *)&m, 0.f));
}
TEST(d3dx_redist_optional_x9_vec3_normalize_length) {
    NEED_MATH(); REQUIRE(F9(D3DXVec3Normalize));
    D3DXVECTOR3 v = { 3, 4, 12 }, o;
    CHECKN(dx_vec3_length(&v), 13.f);
    CHECK(F9(D3DXVec3Normalize)(&o, &v) == &o);
    CHECKN(dx_vec3_length(&o), 1.f);
    CHECKN(o.x, 3.f / 13); CHECKN(o.y, 4.f / 13); CHECKN(o.z, 12.f / 13);
}
TEST(d3dx_redist_optional_x9_vec3_cross_dot) {
    /* Vec3Cross / Vec3Dot are header-inline in D3DX 9.43 (not exported, probed): check the local helpers against the exported Normalize */
    NEED_MATH(); REQUIRE(F9(D3DXVec3Normalize));
    HMODULE xm = GetModuleHandleA(X9);
    CHECK(GetProcAddress(xm, "D3DXVec3Cross") == NULL); CHECK(GetProcAddress(xm, "D3DXVec3Dot") == NULL);
    D3DXVECTOR3 x = {1,0,0}, y = {0,1,0}, z, n;
    dx_vec3_cross(&z, &x, &y);
    CHECKN(z.x, 0.f); CHECKN(z.y, 0.f); CHECKN(z.z, 1.f);
    dx_vec3_cross(&z, &y, &x);
    CHECKN(z.z, -1.f);
    CHECKN(dx_vec3_dot(&x, &y), 0.f);
    CHECKN(dx_vec3_dot(&x, &x), 1.f);
    F9(D3DXVec3Normalize)(&n, &z); CHECKN(dx_vec3_dot(&n, &n), 1.f);
}
