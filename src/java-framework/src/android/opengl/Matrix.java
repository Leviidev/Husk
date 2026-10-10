package android.opengl;

/** 4x4 column-major float matrices, as OpenGL ES uses them. */
public class Matrix {
    private static final float[] sTemp = new float[32];
    @Deprecated public Matrix() {}
    public static void multiplyMM(float[] result, int resultOffset, float[] lhs, int lhsOffset, float[] rhs, int rhsOffset) {
        if (result == null || lhs == null || rhs == null) throw new IllegalArgumentException("null array");
        float[] r = new float[16];
        for (int i = 0; i < 4; i++) {
            final float rhs_i0 = rhs[rhsOffset + i * 4], rhs_i1 = rhs[rhsOffset + i * 4 + 1], rhs_i2 = rhs[rhsOffset + i * 4 + 2], rhs_i3 = rhs[rhsOffset + i * 4 + 3];
            for (int j = 0; j < 4; j++) r[i * 4 + j] = lhs[lhsOffset + j] * rhs_i0 + lhs[lhsOffset + 4 + j] * rhs_i1 + lhs[lhsOffset + 8 + j] * rhs_i2 + lhs[lhsOffset + 12 + j] * rhs_i3;
        }
        System.arraycopy(r, 0, result, resultOffset, 16);
    }
    public static void multiplyMV(float[] resultVec, int resultVecOffset, float[] lhsMat, int lhsMatOffset, float[] rhsVec, int rhsVecOffset) {
        float x = rhsVec[rhsVecOffset], y = rhsVec[rhsVecOffset + 1], z = rhsVec[rhsVecOffset + 2], w = rhsVec[rhsVecOffset + 3];
        float[] r = new float[4];
        for (int i = 0; i < 4; i++) r[i] = lhsMat[lhsMatOffset + i] * x + lhsMat[lhsMatOffset + 4 + i] * y + lhsMat[lhsMatOffset + 8 + i] * z + lhsMat[lhsMatOffset + 12 + i] * w;
        System.arraycopy(r, 0, resultVec, resultVecOffset, 4);
    }
    public static void transposeM(float[] mTrans, int mTransOffset, float[] m, int mOffset) {
        for (int i = 0; i < 4; i++) { int mBase = i * 4 + mOffset; mTrans[i + mTransOffset] = m[mBase]; mTrans[i + 4 + mTransOffset] = m[mBase + 1]; mTrans[i + 8 + mTransOffset] = m[mBase + 2]; mTrans[i + 12 + mTransOffset] = m[mBase + 3]; }
    }
    public static boolean invertM(float[] mInv, int mInvOffset, float[] m, int mOffset) {
        final float src0 = m[mOffset], src4 = m[mOffset + 1], src8 = m[mOffset + 2], src12 = m[mOffset + 3];
        final float src1 = m[mOffset + 4], src5 = m[mOffset + 5], src9 = m[mOffset + 6], src13 = m[mOffset + 7];
        final float src2 = m[mOffset + 8], src6 = m[mOffset + 9], src10 = m[mOffset + 10], src14 = m[mOffset + 11];
        final float src3 = m[mOffset + 12], src7 = m[mOffset + 13], src11 = m[mOffset + 14], src15 = m[mOffset + 15];
        final float atmp0 = src10 * src15, atmp1 = src11 * src14, atmp2 = src9 * src15, atmp3 = src11 * src13, atmp4 = src9 * src14, atmp5 = src10 * src13, atmp6 = src8 * src15, atmp7 = src11 * src12, atmp8 = src8 * src14, atmp9 = src10 * src12, atmp10 = src8 * src13, atmp11 = src9 * src12;
        final float dst0 = (atmp0 * src5 + atmp3 * src6 + atmp4 * src7) - (atmp1 * src5 + atmp2 * src6 + atmp5 * src7);
        final float dst1 = (atmp1 * src4 + atmp6 * src6 + atmp9 * src7) - (atmp0 * src4 + atmp7 * src6 + atmp8 * src7);
        final float dst2 = (atmp2 * src4 + atmp7 * src5 + atmp10 * src7) - (atmp3 * src4 + atmp6 * src5 + atmp11 * src7);
        final float dst3 = (atmp5 * src4 + atmp8 * src5 + atmp11 * src6) - (atmp4 * src4 + atmp9 * src5 + atmp10 * src6);
        final float dst4 = (atmp1 * src1 + atmp2 * src2 + atmp5 * src3) - (atmp0 * src1 + atmp3 * src2 + atmp4 * src3);
        final float dst5 = (atmp0 * src0 + atmp7 * src2 + atmp8 * src3) - (atmp1 * src0 + atmp6 * src2 + atmp9 * src3);
        final float dst6 = (atmp3 * src0 + atmp6 * src1 + atmp11 * src3) - (atmp2 * src0 + atmp7 * src1 + atmp10 * src3);
        final float dst7 = (atmp4 * src0 + atmp9 * src1 + atmp10 * src2) - (atmp5 * src0 + atmp8 * src1 + atmp11 * src2);
        final float btmp0 = src2 * src7, btmp1 = src3 * src6, btmp2 = src1 * src7, btmp3 = src3 * src5, btmp4 = src1 * src6, btmp5 = src2 * src5, btmp6 = src0 * src7, btmp7 = src3 * src4, btmp8 = src0 * src6, btmp9 = src2 * src4, btmp10 = src0 * src5, btmp11 = src1 * src4;
        final float dst8 = (btmp0 * src13 + btmp3 * src14 + btmp4 * src15) - (btmp1 * src13 + btmp2 * src14 + btmp5 * src15);
        final float dst9 = (btmp1 * src12 + btmp6 * src14 + btmp9 * src15) - (btmp0 * src12 + btmp7 * src14 + btmp8 * src15);
        final float dst10 = (btmp2 * src12 + btmp7 * src13 + btmp10 * src15) - (btmp3 * src12 + btmp6 * src13 + btmp11 * src15);
        final float dst11 = (btmp5 * src12 + btmp8 * src13 + btmp11 * src14) - (btmp4 * src12 + btmp9 * src13 + btmp10 * src14);
        final float dst12 = (btmp2 * src10 + btmp5 * src11 + btmp1 * src9) - (btmp4 * src11 + btmp0 * src9 + btmp3 * src10);
        final float dst13 = (btmp8 * src11 + btmp0 * src8 + btmp7 * src10) - (btmp6 * src10 + btmp9 * src11 + btmp1 * src8);
        final float dst14 = (btmp6 * src9 + btmp11 * src11 + btmp3 * src8) - (btmp10 * src11 + btmp2 * src8 + btmp7 * src9);
        final float dst15 = (btmp10 * src10 + btmp4 * src8 + btmp9 * src9) - (btmp8 * src9 + btmp11 * src10 + btmp5 * src8);
        final float det = src0 * dst0 + src1 * dst1 + src2 * dst2 + src3 * dst3;
        if (det == 0.0f) return false;
        final float invdet = 1.0f / det;
        mInv[mInvOffset] = dst0 * invdet; mInv[1 + mInvOffset] = dst1 * invdet; mInv[2 + mInvOffset] = dst2 * invdet; mInv[3 + mInvOffset] = dst3 * invdet;
        mInv[4 + mInvOffset] = dst4 * invdet; mInv[5 + mInvOffset] = dst5 * invdet; mInv[6 + mInvOffset] = dst6 * invdet; mInv[7 + mInvOffset] = dst7 * invdet;
        mInv[8 + mInvOffset] = dst8 * invdet; mInv[9 + mInvOffset] = dst9 * invdet; mInv[10 + mInvOffset] = dst10 * invdet; mInv[11 + mInvOffset] = dst11 * invdet;
        mInv[12 + mInvOffset] = dst12 * invdet; mInv[13 + mInvOffset] = dst13 * invdet; mInv[14 + mInvOffset] = dst14 * invdet; mInv[15 + mInvOffset] = dst15 * invdet;
        return true;
    }
    public static void orthoM(float[] m, int mOffset, float left, float right, float bottom, float top, float near, float far) {
        if (left == right) throw new IllegalArgumentException("left == right");
        if (bottom == top) throw new IllegalArgumentException("bottom == top");
        if (near == far) throw new IllegalArgumentException("near == far");
        final float r_width = 1.0f / (right - left), r_height = 1.0f / (top - bottom), r_depth = 1.0f / (far - near);
        final float x = 2.0f * (r_width), y = 2.0f * (r_height), z = -2.0f * (r_depth);
        final float tx = -(right + left) * r_width, ty = -(top + bottom) * r_height, tz = -(far + near) * r_depth;
        m[mOffset] = x; m[mOffset + 5] = y; m[mOffset + 10] = z; m[mOffset + 12] = tx; m[mOffset + 13] = ty; m[mOffset + 14] = tz; m[mOffset + 15] = 1.0f;
        m[mOffset + 1] = 0; m[mOffset + 2] = 0; m[mOffset + 3] = 0; m[mOffset + 4] = 0; m[mOffset + 6] = 0; m[mOffset + 7] = 0; m[mOffset + 8] = 0; m[mOffset + 9] = 0; m[mOffset + 11] = 0;
    }
    public static void frustumM(float[] m, int offset, float left, float right, float bottom, float top, float near, float far) {
        if (left == right) throw new IllegalArgumentException("left == right");
        if (top == bottom) throw new IllegalArgumentException("top == bottom");
        if (near == far) throw new IllegalArgumentException("near == far");
        if (near <= 0.0f) throw new IllegalArgumentException("near <= 0.0f");
        if (far <= 0.0f) throw new IllegalArgumentException("far <= 0.0f");
        final float r_width = 1.0f / (right - left), r_height = 1.0f / (top - bottom), r_depth = 1.0f / (near - far);
        final float x = 2.0f * (near * r_width), y = 2.0f * (near * r_height), A = (right + left) * r_width, B = (top + bottom) * r_height, C = (far + near) * r_depth, D = 2.0f * (far * near * r_depth);
        m[offset] = x; m[offset + 5] = y; m[offset + 8] = A; m[offset + 9] = B; m[offset + 10] = C; m[offset + 14] = D; m[offset + 11] = -1.0f;
        m[offset + 1] = 0; m[offset + 2] = 0; m[offset + 3] = 0; m[offset + 4] = 0; m[offset + 6] = 0; m[offset + 7] = 0; m[offset + 12] = 0; m[offset + 13] = 0; m[offset + 15] = 0;
    }
    public static void perspectiveM(float[] m, int offset, float fovy, float aspect, float zNear, float zFar) {
        float f = 1.0f / (float) Math.tan(fovy * (Math.PI / 360.0)), rangeReciprocal = 1.0f / (zNear - zFar);
        m[offset] = f / aspect; m[offset + 1] = 0; m[offset + 2] = 0; m[offset + 3] = 0;
        m[offset + 4] = 0; m[offset + 5] = f; m[offset + 6] = 0; m[offset + 7] = 0;
        m[offset + 8] = 0; m[offset + 9] = 0; m[offset + 10] = (zFar + zNear) * rangeReciprocal; m[offset + 11] = -1.0f;
        m[offset + 12] = 0; m[offset + 13] = 0; m[offset + 14] = 2.0f * zFar * zNear * rangeReciprocal; m[offset + 15] = 0;
    }
    public static float length(float x, float y, float z) { return (float) Math.sqrt(x * x + y * y + z * z); }
    public static void setIdentityM(float[] sm, int smOffset) { for (int i = 0; i < 16; i++) sm[smOffset + i] = 0; for (int i = 0; i < 16; i += 5) sm[smOffset + i] = 1.0f; }
    public static void scaleM(float[] sm, int smOffset, float[] m, int mOffset, float x, float y, float z) {
        for (int i = 0; i < 4; i++) { int smi = smOffset + i, mi = mOffset + i; sm[smi] = m[mi] * x; sm[4 + smi] = m[4 + mi] * y; sm[8 + smi] = m[8 + mi] * z; sm[12 + smi] = m[12 + mi]; }
    }
    public static void scaleM(float[] m, int mOffset, float x, float y, float z) { for (int i = 0; i < 4; i++) { int mi = mOffset + i; m[mi] *= x; m[4 + mi] *= y; m[8 + mi] *= z; } }
    public static void translateM(float[] tm, int tmOffset, float[] m, int mOffset, float x, float y, float z) {
        for (int i = 0; i < 12; i++) tm[tmOffset + i] = m[mOffset + i];
        for (int i = 0; i < 4; i++) { int tmi = tmOffset + i, mi = mOffset + i; tm[12 + tmi] = m[mi] * x + m[4 + mi] * y + m[8 + mi] * z + m[12 + mi]; }
    }
    public static void translateM(float[] m, int mOffset, float x, float y, float z) { for (int i = 0; i < 4; i++) { int mi = mOffset + i; m[12 + mi] += m[mi] * x + m[4 + mi] * y + m[8 + mi] * z; } }
    public static void rotateM(float[] rm, int rmOffset, float[] m, int mOffset, float a, float x, float y, float z) {
        float[] r = new float[16];
        setRotateM(r, 0, a, x, y, z);
        multiplyMM(rm, rmOffset, m, mOffset, r, 0);
    }
    public static void rotateM(float[] m, int mOffset, float a, float x, float y, float z) {
        float[] t = new float[32];
        setRotateM(t, 0, a, x, y, z);
        multiplyMM(t, 16, m, mOffset, t, 0);
        System.arraycopy(t, 16, m, mOffset, 16);
    }
    public static void setRotateM(float[] rm, int rmOffset, float a, float x, float y, float z) {
        rm[rmOffset + 3] = 0; rm[rmOffset + 7] = 0; rm[rmOffset + 11] = 0; rm[rmOffset + 12] = 0; rm[rmOffset + 13] = 0; rm[rmOffset + 14] = 0; rm[rmOffset + 15] = 1;
        a *= (float) (Math.PI / 180.0f);
        float s = (float) Math.sin(a), c = (float) Math.cos(a);
        if (1.0f == x && 0.0f == y && 0.0f == z) { rm[rmOffset + 5] = c; rm[rmOffset + 10] = c; rm[rmOffset + 6] = s; rm[rmOffset + 9] = -s; rm[rmOffset + 1] = 0; rm[rmOffset + 2] = 0; rm[rmOffset + 4] = 0; rm[rmOffset + 8] = 0; rm[rmOffset] = 1; }
        else if (0.0f == x && 1.0f == y && 0.0f == z) { rm[rmOffset] = c; rm[rmOffset + 10] = c; rm[rmOffset + 8] = s; rm[rmOffset + 2] = -s; rm[rmOffset + 1] = 0; rm[rmOffset + 4] = 0; rm[rmOffset + 6] = 0; rm[rmOffset + 9] = 0; rm[rmOffset + 5] = 1; }
        else if (0.0f == x && 0.0f == y && 1.0f == z) { rm[rmOffset] = c; rm[rmOffset + 5] = c; rm[rmOffset + 1] = s; rm[rmOffset + 4] = -s; rm[rmOffset + 2] = 0; rm[rmOffset + 6] = 0; rm[rmOffset + 8] = 0; rm[rmOffset + 9] = 0; rm[rmOffset + 10] = 1; }
        else {
            float len = length(x, y, z);
            if (1.0f != len) { float recipLen = 1.0f / len; x *= recipLen; y *= recipLen; z *= recipLen; }
            float nc = 1.0f - c, xy = x * y, yz = y * z, zx = z * x, xs = x * s, ys = y * s, zs = z * s;
            rm[rmOffset] = x * x * nc + c; rm[rmOffset + 4] = xy * nc - zs; rm[rmOffset + 8] = zx * nc + ys;
            rm[rmOffset + 1] = xy * nc + zs; rm[rmOffset + 5] = y * y * nc + c; rm[rmOffset + 9] = yz * nc - xs;
            rm[rmOffset + 2] = zx * nc - ys; rm[rmOffset + 6] = yz * nc + xs; rm[rmOffset + 10] = z * z * nc + c;
        }
    }
    @Deprecated public static void setRotateEulerM(float[] rm, int rmOffset, float x, float y, float z) { setRotateEulerM2(rm, rmOffset, x, y, z); }
    public static void setRotateEulerM2(float[] rm, int rmOffset, float x, float y, float z) {
        x *= (float) (Math.PI / 180.0f); y *= (float) (Math.PI / 180.0f); z *= (float) (Math.PI / 180.0f);
        float cx = (float) Math.cos(x), sx = (float) Math.sin(x), cy = (float) Math.cos(y), sy = (float) Math.sin(y), cz = (float) Math.cos(z), sz = (float) Math.sin(z), cxsy = cx * sy, sxsy = sx * sy;
        rm[rmOffset] = cy * cz; rm[rmOffset + 1] = -cy * sz; rm[rmOffset + 2] = sy; rm[rmOffset + 3] = 0.0f;
        rm[rmOffset + 4] = sxsy * cz + cx * sz; rm[rmOffset + 5] = -sxsy * sz + cx * cz; rm[rmOffset + 6] = -sx * cy; rm[rmOffset + 7] = 0.0f;
        rm[rmOffset + 8] = -cxsy * cz + sx * sz; rm[rmOffset + 9] = cxsy * sz + sx * cz; rm[rmOffset + 10] = cx * cy; rm[rmOffset + 11] = 0.0f;
        rm[rmOffset + 12] = 0.0f; rm[rmOffset + 13] = 0.0f; rm[rmOffset + 14] = 0.0f; rm[rmOffset + 15] = 1.0f;
    }
    public static void setLookAtM(float[] rm, int rmOffset, float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
        float fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
        float rlf = 1.0f / Matrix.length(fx, fy, fz); fx *= rlf; fy *= rlf; fz *= rlf;
        float sx = fy * upZ - fz * upY, sy = fz * upX - fx * upZ, sz = fx * upY - fy * upX;
        float rls = 1.0f / Matrix.length(sx, sy, sz); sx *= rls; sy *= rls; sz *= rls;
        float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
        rm[rmOffset] = sx; rm[rmOffset + 1] = ux; rm[rmOffset + 2] = -fx; rm[rmOffset + 3] = 0.0f;
        rm[rmOffset + 4] = sy; rm[rmOffset + 5] = uy; rm[rmOffset + 6] = -fy; rm[rmOffset + 7] = 0.0f;
        rm[rmOffset + 8] = sz; rm[rmOffset + 9] = uz; rm[rmOffset + 10] = -fz; rm[rmOffset + 11] = 0.0f;
        rm[rmOffset + 12] = 0.0f; rm[rmOffset + 13] = 0.0f; rm[rmOffset + 14] = 0.0f; rm[rmOffset + 15] = 1.0f;
        translateM(rm, rmOffset, -eyeX, -eyeY, -eyeZ);
    }
}
