#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
android.opengl.GLES20 for Husk's Java framework: generates the Java class (static natives) and their C implementations, which call
OpenGL ES through ANGLE (tl_egl_resolve). One spec, so the two never disagree.

    python3 gen/gles20.py <java-out-dir> <c-out-file>
"""
import sys, os

# return name(params): params are int float boolean String, T[] (array + int offset follows), Buffer types
SPEC = """
void glActiveTexture(int texture)
void glAttachShader(int program, int shader)
void glBindAttribLocation(int program, int index, String name)
void glBindBuffer(int target, int buffer)
void glBindFramebuffer(int target, int framebuffer)
void glBindRenderbuffer(int target, int renderbuffer)
void glBindTexture(int target, int texture)
void glBlendColor(float red, float green, float blue, float alpha)
void glBlendEquation(int mode)
void glBlendEquationSeparate(int modeRGB, int modeAlpha)
void glBlendFunc(int sfactor, int dfactor)
void glBlendFuncSeparate(int srcRGB, int dstRGB, int srcAlpha, int dstAlpha)
void glBufferData(int target, int size, Buffer data, int usage)
void glBufferSubData(int target, int offset, int size, Buffer data)
int glCheckFramebufferStatus(int target)
void glClear(int mask)
void glClearColor(float red, float green, float blue, float alpha)
void glClearDepthf(float depth)
void glClearStencil(int s)
void glColorMask(boolean red, boolean green, boolean blue, boolean alpha)
void glCompileShader(int shader)
void glCompressedTexImage2D(int target, int level, int internalformat, int width, int height, int border, int imageSize, Buffer data)
void glCompressedTexSubImage2D(int target, int level, int xoffset, int yoffset, int width, int height, int format, int imageSize, Buffer data)
void glCopyTexImage2D(int target, int level, int internalformat, int x, int y, int width, int height, int border)
void glCopyTexSubImage2D(int target, int level, int xoffset, int yoffset, int x, int y, int width, int height)
int glCreateProgram()
int glCreateShader(int type)
void glCullFace(int mode)
void glDeleteBuffers(int n, int[] buffers)
void glDeleteBuffers(int n, IntBuffer buffers)
void glDeleteFramebuffers(int n, int[] framebuffers)
void glDeleteFramebuffers(int n, IntBuffer framebuffers)
void glDeleteProgram(int program)
void glDeleteRenderbuffers(int n, int[] renderbuffers)
void glDeleteRenderbuffers(int n, IntBuffer renderbuffers)
void glDeleteShader(int shader)
void glDeleteTextures(int n, int[] textures)
void glDeleteTextures(int n, IntBuffer textures)
void glDepthFunc(int func)
void glDepthMask(boolean flag)
void glDepthRangef(float zNear, float zFar)
void glDetachShader(int program, int shader)
void glDisable(int cap)
void glDisableVertexAttribArray(int index)
void glDrawArrays(int mode, int first, int count)
void glDrawElements(int mode, int count, int type, int offset)
void glDrawElements(int mode, int count, int type, Buffer indices)
void glEnable(int cap)
void glEnableVertexAttribArray(int index)
void glFinish()
void glFlush()
void glFramebufferRenderbuffer(int target, int attachment, int renderbuffertarget, int renderbuffer)
void glFramebufferTexture2D(int target, int attachment, int textarget, int texture, int level)
void glFrontFace(int mode)
void glGenBuffers(int n, int[] buffers)
void glGenBuffers(int n, IntBuffer buffers)
void glGenerateMipmap(int target)
void glGenFramebuffers(int n, int[] framebuffers)
void glGenFramebuffers(int n, IntBuffer framebuffers)
void glGenRenderbuffers(int n, int[] renderbuffers)
void glGenRenderbuffers(int n, IntBuffer renderbuffers)
void glGenTextures(int n, int[] textures)
void glGenTextures(int n, IntBuffer textures)
void glGetAttachedShaders(int program, int maxcount, int[] count, int[] shaders)
int glGetAttribLocation(int program, String name)
void glGetBooleanv(int pname, boolean[] params)
void glGetBooleanv(int pname, IntBuffer params)
void glGetBufferParameteriv(int target, int pname, int[] params)
void glGetBufferParameteriv(int target, int pname, IntBuffer params)
int glGetError()
void glGetFloatv(int pname, float[] params)
void glGetFloatv(int pname, FloatBuffer params)
void glGetFramebufferAttachmentParameteriv(int target, int attachment, int pname, int[] params)
void glGetFramebufferAttachmentParameteriv(int target, int attachment, int pname, IntBuffer params)
void glGetIntegerv(int pname, int[] params)
void glGetIntegerv(int pname, IntBuffer params)
void glGetProgramiv(int program, int pname, int[] params)
void glGetProgramiv(int program, int pname, IntBuffer params)
void glGetRenderbufferParameteriv(int target, int pname, int[] params)
void glGetRenderbufferParameteriv(int target, int pname, IntBuffer params)
void glGetShaderiv(int shader, int pname, int[] params)
void glGetShaderiv(int shader, int pname, IntBuffer params)
void glGetShaderPrecisionFormat(int shadertype, int precisiontype, int[] range, int[] precision)
void glGetShaderPrecisionFormat(int shadertype, int precisiontype, IntBuffer range, IntBuffer precision)
void glGetTexParameterfv(int target, int pname, float[] params)
void glGetTexParameterfv(int target, int pname, FloatBuffer params)
void glGetTexParameteriv(int target, int pname, int[] params)
void glGetTexParameteriv(int target, int pname, IntBuffer params)
void glGetUniformfv(int program, int location, float[] params)
void glGetUniformfv(int program, int location, FloatBuffer params)
void glGetUniformiv(int program, int location, int[] params)
void glGetUniformiv(int program, int location, IntBuffer params)
int glGetUniformLocation(int program, String name)
void glGetVertexAttribfv(int index, int pname, float[] params)
void glGetVertexAttribfv(int index, int pname, FloatBuffer params)
void glGetVertexAttribiv(int index, int pname, int[] params)
void glGetVertexAttribiv(int index, int pname, IntBuffer params)
void glHint(int target, int mode)
boolean glIsBuffer(int buffer)
boolean glIsEnabled(int cap)
boolean glIsFramebuffer(int framebuffer)
boolean glIsProgram(int program)
boolean glIsRenderbuffer(int renderbuffer)
boolean glIsShader(int shader)
boolean glIsTexture(int texture)
void glLineWidth(float width)
void glLinkProgram(int program)
void glPixelStorei(int pname, int param)
void glPolygonOffset(float factor, float units)
void glReadPixels(int x, int y, int width, int height, int format, int type, Buffer pixels)
void glReleaseShaderCompiler()
void glRenderbufferStorage(int target, int internalformat, int width, int height)
void glSampleCoverage(float value, boolean invert)
void glScissor(int x, int y, int width, int height)
void glStencilFunc(int func, int ref, int mask)
void glStencilFuncSeparate(int face, int func, int ref, int mask)
void glStencilMask(int mask)
void glStencilMaskSeparate(int face, int mask)
void glStencilOp(int fail, int zfail, int zpass)
void glStencilOpSeparate(int face, int fail, int zfail, int zpass)
void glTexImage2D(int target, int level, int internalformat, int width, int height, int border, int format, int type, Buffer pixels)
void glTexParameterf(int target, int pname, float param)
void glTexParameterfv(int target, int pname, float[] params)
void glTexParameterfv(int target, int pname, FloatBuffer params)
void glTexParameteri(int target, int pname, int param)
void glTexParameteriv(int target, int pname, int[] params)
void glTexParameteriv(int target, int pname, IntBuffer params)
void glTexSubImage2D(int target, int level, int xoffset, int yoffset, int width, int height, int format, int type, Buffer pixels)
void glUniform1f(int location, float x)
void glUniform1fv(int location, int count, float[] v)
void glUniform1fv(int location, int count, FloatBuffer v)
void glUniform1i(int location, int x)
void glUniform1iv(int location, int count, int[] v)
void glUniform1iv(int location, int count, IntBuffer v)
void glUniform2f(int location, float x, float y)
void glUniform2fv(int location, int count, float[] v)
void glUniform2fv(int location, int count, FloatBuffer v)
void glUniform2i(int location, int x, int y)
void glUniform2iv(int location, int count, int[] v)
void glUniform2iv(int location, int count, IntBuffer v)
void glUniform3f(int location, float x, float y, float z)
void glUniform3fv(int location, int count, float[] v)
void glUniform3fv(int location, int count, FloatBuffer v)
void glUniform3i(int location, int x, int y, int z)
void glUniform3iv(int location, int count, int[] v)
void glUniform3iv(int location, int count, IntBuffer v)
void glUniform4f(int location, float x, float y, float z, float w)
void glUniform4fv(int location, int count, float[] v)
void glUniform4fv(int location, int count, FloatBuffer v)
void glUniform4i(int location, int x, int y, int z, int w)
void glUniform4iv(int location, int count, int[] v)
void glUniform4iv(int location, int count, IntBuffer v)
void glUniformMatrix2fv(int location, int count, boolean transpose, float[] value)
void glUniformMatrix2fv(int location, int count, boolean transpose, FloatBuffer value)
void glUniformMatrix3fv(int location, int count, boolean transpose, float[] value)
void glUniformMatrix3fv(int location, int count, boolean transpose, FloatBuffer value)
void glUniformMatrix4fv(int location, int count, boolean transpose, float[] value)
void glUniformMatrix4fv(int location, int count, boolean transpose, FloatBuffer value)
void glUseProgram(int program)
void glValidateProgram(int program)
void glVertexAttrib1f(int indx, float x)
void glVertexAttrib1fv(int indx, float[] values)
void glVertexAttrib1fv(int indx, FloatBuffer values)
void glVertexAttrib2f(int indx, float x, float y)
void glVertexAttrib2fv(int indx, float[] values)
void glVertexAttrib2fv(int indx, FloatBuffer values)
void glVertexAttrib3f(int indx, float x, float y, float z)
void glVertexAttrib3fv(int indx, float[] values)
void glVertexAttrib3fv(int indx, FloatBuffer values)
void glVertexAttrib4f(int indx, float x, float y, float z, float w)
void glVertexAttrib4fv(int indx, float[] values)
void glVertexAttrib4fv(int indx, FloatBuffer values)
void glVertexAttribPointer(int indx, int size, int type, boolean normalized, int stride, int offset)
void glVertexAttribPointer(int indx, int size, int type, boolean normalized, int stride, Buffer ptr)
void glViewport(int x, int y, int width, int height)
"""
# Special methods written by hand (string results, the attrib/uniform queries): Java signature, C body name.
SPECIAL_JAVA = """
    public static native String glGetString(int name);
    public static native void glShaderSource(int shader, String string);
    public static native String glGetProgramInfoLog(int program);
    public static native String glGetShaderInfoLog(int shader);
    public static native String glGetShaderSource(int shader);
    public static native String glGetActiveAttrib(int program, int index, int[] size, int sizeOffset, int[] type, int typeOffset);
    public static native String glGetActiveAttrib(int program, int index, java.nio.IntBuffer size, java.nio.IntBuffer type);
    public static native String glGetActiveUniform(int program, int index, int[] size, int sizeOffset, int[] type, int typeOffset);
    public static native String glGetActiveUniform(int program, int index, java.nio.IntBuffer size, java.nio.IntBuffer type);
    public static native void glGetActiveAttrib(int program, int index, int bufsize, int[] length, int lengthOffset, int[] size, int sizeOffset, int[] type, int typeOffset, byte[] name, int nameOffset);
    public static native void glGetActiveUniform(int program, int index, int bufsize, int[] length, int lengthOffset, int[] size, int sizeOffset, int[] type, int typeOffset, byte[] name, int nameOffset);
"""

JTYPE = {'int': 'I', 'float': 'F', 'boolean': 'Z', 'String': 'Ljava/lang/String;', 'int[]': '[I', 'float[]': '[F', 'boolean[]': '[Z', 'byte[]': '[B',
         'Buffer': 'Ljava/nio/Buffer;', 'IntBuffer': 'Ljava/nio/IntBuffer;', 'FloatBuffer': 'Ljava/nio/FloatBuffer;', 'void': 'V'}
CTYPE = {'int': 'int32_t', 'float': 'float', 'boolean': 'uint8_t', 'String': 'const char *', 'int[]': 'void *', 'float[]': 'void *', 'boolean[]': 'void *',
         'byte[]': 'void *', 'Buffer': 'void *', 'IntBuffer': 'void *', 'FloatBuffer': 'void *'}

def parse():
    out = []
    for line in SPEC.strip().splitlines():
        ret, rest = line.split(' ', 1)
        name, args = rest.split('(')
        args = args.rstrip(')').strip()
        params = []
        if args:
            for a in args.split(','):
                t, n = a.strip().rsplit(' ', 1)
                params.append((t, n))
        out.append((ret, name, params))
    return out

def java(methods, outdir):
    os.makedirs(outdir, exist_ok=True)
    L = ['// Generated by gen/gles20.py -- do not edit.', 'package android.opengl;', '', 'import java.nio.*;', '',
         'public class GLES20 {']
    consts = open(os.path.join(os.path.dirname(__file__), 'gles20-constants.txt')).read().split()
    for i in range(0, len(consts), 2):
        L.append(f'    public static final int {consts[i]} = {consts[i+1]};')
    for ret, name, params in methods:
        jp = []
        for t, n in params:
            jp.append(f'{t} {n}')
            if t.endswith('[]'): jp.append(f'int {n}Offset')
        L.append(f'    public static native {ret} {name}({", ".join(jp)});')
    L.append(SPECIAL_JAVA)
    L.append('}')
    open(os.path.join(outdir, 'GLES20.java'), 'w').write('\n'.join(L) + '\n')

# GL takes these as GLsizeiptr / GLintptr (64-bit) or as a pointer made from an offset into a bound buffer
WIDE = {('glBufferData', 1): 'intptr_t', ('glBufferSubData', 1): 'intptr_t', ('glBufferSubData', 2): 'intptr_t',
        ('glDrawElements', 3): 'ptr', ('glVertexAttribPointer', 5): 'ptr'}

def csig(ret, params):
    s = '('
    for t, n in params:
        s += JTYPE[t]
        if t.endswith('[]'): s += 'I'
    return s + ')' + JTYPE[ret]

def c(methods, outfile):
    L = ['/* SPDX-License-Identifier: GPL-2.0-or-later */',
         '/* android.opengl.GLES20\'s natives, generated by src/java-framework/gen/gles20.py from the same list as the Java class. */',
         '#include "husk-tl-dvm-gl.h"', '']
    table = []
    seen = {}
    for ret, name, params in methods:
        key = name + csig(ret, params)
        fn = f'G_{name}_{seen.get(name, 0)}'
        seen[name] = seen.get(name, 0) + 1
        ct = ', '.join(('void *' if WIDE.get((name, k)) == 'ptr' else WIDE.get((name, k), CTYPE[t])) for k, (t, n) in enumerate(params))
        ptr = f'{"int32_t" if ret == "int" else "uint8_t" if ret == "boolean" else "void"} (*)({ct if ct else "void"})'
        L.append(f'static bool {fn}(jobj *self, const jvalue *a, jvalue *ret)')
        L.append('{')
        L.append('    (void)self; (void)a; (void)ret;')
        L.append(f'    static {ptr.replace("(*)", "(*f)")};')
        L.append(f'    if (!f) f = ({ptr})gl_fn("{name}");')
        L.append('    if (!f) return true;')
        args = []
        i = 0
        for k, (t, n) in enumerate(params):
            w = WIDE.get((name, k))
            if t == 'int' and w == 'ptr': args.append(f'(void *)(intptr_t)a[{i}].i')
            elif t == 'int' and w: args.append(f'(intptr_t)a[{i}].i')
            elif t == 'int': args.append(f'a[{i}].i')
            elif t == 'float': args.append(f'a[{i}].f')
            elif t == 'boolean': args.append(f'a[{i}].z')
            elif t == 'String':
                L.append(f'    const char *s{i} = gl_str(a[{i}].l);')
                args.append(f's{i}')
            elif t.endswith('[]'):
                args.append(f'gl_array(a[{i}].l, a[{i+1}].i)')
                i += 1
            else:
                args.append(f'gl_buffer(a[{i}].l)')
            i += 1
        call = f'f({", ".join(args)})'
        if ret == 'int': L.append(f'    ret->j = (uint32_t){call};')
        elif ret == 'boolean': L.append(f'    ret->j = {call} ? 1 : 0;')
        else: L.append(f'    {call};')
        L.append('    return true;')
        L.append('}')
        jsig = csig(ret, params)
        table.append(f'    {{ "{name}", "{jsig}", {fn} }},')
    L.append('')
    L.append('const gl_native k_gles20[] = {')
    L += table
    L.append('    { NULL, NULL, NULL },')
    L.append('};')
    open(outfile, 'w').write('\n'.join(L) + '\n')

m = parse()
java(m, sys.argv[1])
c(m, sys.argv[2])
print(f'{len(m)} methods')
