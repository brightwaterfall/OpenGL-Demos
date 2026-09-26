// ocp_rings.cpp
// OpenGL 4.1 core, GLFW + GLEW, no GLM.
//
// Three rotating ring walls with gaps.
// Dot/person escapes through the aligned doorway.
// GUI buttons rotate rings.
// Random rotation every 10 minutes.
// No rotation while dot is in hallway or inside the ring/container zone.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>

using namespace std;
int mouseXPPixels;
// ---------------------------------------------------------------------------
// Math helpers, no GLM
// ---------------------------------------------------------------------------

static const float PI = 3.14159265358979323846f;
static const float DEG = PI / 180.0f;

struct Vec3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4
{
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

static Vec3 operator+(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(Vec3 a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }

static float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(Vec3 a, Vec3 b)
{
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static Vec3 normalizeVec(Vec3 v)
{
    float len = sqrtf(dot(v, v));
    if (len < 1e-8f)
        return Vec3{0.0f, 0.0f, 1.0f};
    return v * (1.0f / len);
}

struct Mat4
{
    // Column-major, compatible with OpenGL.
    float m[16] = {};
};

static Mat4 identity()
{
    Mat4 r;
    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[15] = 1.0f;
    return r;
}

static Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int c = 0; c < 4; ++c)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a.m[row + k * 4] * b.m[k + c * 4];
            r.m[row + c * 4] = sum;
        }
    }
    return r;
}

static Mat4 perspective(float fovyRad, float aspect, float zNear, float zFar)
{
    Mat4 r;
    float f = 1.0f / tanf(fovyRad * 0.5f);

    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    r.m[15] = 0.0f;

    return r;
}

static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up)
{
    Vec3 f = normalizeVec(center - eye);
    Vec3 s = normalizeVec(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 r = identity();

    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[12] = -dot(s, eye);

    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[13] = -dot(u, eye);

    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[14] = dot(f, eye);

    r.m[3] = 0.0f;
    r.m[7] = 0.0f;
    r.m[11] = 0.0f;
    r.m[15] = 1.0f;

    return r;
}

static Mat4 rotateZ(float angle)
{
    Mat4 r = identity();
    float c = cosf(angle);
    float s = sinf(angle);

    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;

    return r;
}

static Mat4 translate(float x, float y, float z)
{
    Mat4 r = identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

static Mat4 ortho2D(float left, float right, float bottom, float top, float zNear, float zFar)
{
    Mat4 r;

    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (zFar - zNear);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(zFar + zNear) / (zFar - zNear);
    r.m[15] = 1.0f;

    return r;
}

static float norm01(float a)
{
    const float twoPi = 2.0f * PI;
    a = fmodf(a, twoPi);
    if (a < 0.0f) a += twoPi;
    return a;
}

static float normSigned(float a)
{
    a = norm01(a);
    if (a > PI) a -= 2.0f * PI;
    return a;
}

// ---------------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------------

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char info[1024];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        fprintf(stderr, "Shader compile error:\n%s\n", info);
    }

    return shader;
}

static GLuint linkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char info[1024];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        fprintf(stderr, "Program link error:\n%s\n", info);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

struct Program
{
    GLuint id = 0;

    GLint proj = -1;
    GLint view = -1;
    GLint model = -1;
    GLint color = -1;
    GLint alpha = -1;
    GLint useLighting = -1;
    GLint lightDir = -1;
    GLint camPos = -1;
};

static Program makeProgram(const char* vsSrc, const char* fsSrc, bool isMainProgram)
{
    Program p;
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    p.id = linkProgram(vs, fs);

    p.proj = glGetUniformLocation(p.id, "uProj");

    if (isMainProgram)
    {
        p.view = glGetUniformLocation(p.id, "uView");
        p.model = glGetUniformLocation(p.id, "uModel");
        p.color = glGetUniformLocation(p.id, "uColor");
        p.alpha = glGetUniformLocation(p.id, "uAlpha");
        p.useLighting = glGetUniformLocation(p.id, "uUseLighting");
        p.lightDir = glGetUniformLocation(p.id, "uLightDir");
        p.camPos = glGetUniformLocation(p.id, "uCamPos");
    }

    return p;
}

// ---------------------------------------------------------------------------
// Mesh helpers
// ---------------------------------------------------------------------------

struct Mesh
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei count = 0;

    void create(const vector<float>& data)
    {
        count = (GLsizei)(data.size() / 6);

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(float)), data.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
    }

    void draw() const
    {
        if (count == 0) return;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, count);
    }
};

struct DynamicMesh3D
{
    GLuint vao = 0;
    GLuint vbo = 0;
    vector<float> data;

    void init()
    {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
    }

    void begin()
    {
        data.clear();
    }

    void add(float x, float y, float z, float nx, float ny, float nz)
    {
        data.push_back(x);
        data.push_back(y);
        data.push_back(z);
        data.push_back(nx);
        data.push_back(ny);
        data.push_back(nz);
    }

    void draw(const Program& prog, const Mat4& model, Vec3 color, float alpha, bool lighting)
    {
        if (data.empty()) return;

        glUseProgram(prog.id);
        glUniformMatrix4fv(prog.model, 1, GL_FALSE, &model.m[0]);
        glUniform3fv(prog.color, 1, &color.x);
        glUniform1f(prog.alpha, alpha);
        glUniform1i(prog.useLighting, lighting ? 1 : 0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(float)), data.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(data.size() / 6));
        glBindVertexArray(0);
    }
};

struct Overlay
{
    GLuint vao = 0;
    GLuint vbo = 0;
    Program prog;
    vector<float> data;
    Mat4 proj;

    void init(const Program& p)
    {
        prog = p;

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        // vec2 position + vec4 color = 6 floats
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }

    void setProjection(const Mat4& p)
    {
        proj = p;
    }

    void clear()
    {
        data.clear();
    }

    void addVertex(float x, float y, Vec4 c)
    {
        data.push_back(x);
        data.push_back(y);
        data.push_back(c.x);
        data.push_back(c.y);
        data.push_back(c.z);
        data.push_back(c.w);
    }

    void addTriangle(float x0, float y0,
                     float x1, float y1,
                     float x2, float y2,
                     Vec4 c)
    {
        addVertex(x0, y0, c);
        addVertex(x1, y1, c);
        addVertex(x2, y2, c);
    }

    void addQuadPoints(float x0, float y0,
                       float x1, float y1,
                       float x2, float y2,
                       float x3, float y3,
                       Vec4 c)
    {
        addTriangle(x0, y0, x1, y1, x2, y2, c);
        addTriangle(x0, y0, x2, y2, x3, y3, c);
    }

    void addQuad(float x0, float y0, float x1, float y1, Vec4 c)
    {
        addQuadPoints(x0, y0, x1, y0, x1, y1, x0, y1, c);
    }

    void addCircle(float cx, float cy, float r, int seg, Vec4 c)
    {
        if (seg < 3) seg = 3;

        for (int i = 0; i < seg; ++i)
        {
            float a0 = 2.0f * PI * (float)i / (float)seg;
            float a1 = 2.0f * PI * (float)(i + 1) / (float)seg;

            float x0 = cx + cosf(a0) * r;
            float y0 = cy + sinf(a0) * r;
            float x1 = cx + cosf(a1) * r;
            float y1 = cy + sinf(a1) * r;

            addTriangle(cx, cy, x0, y0, x1, y1, c);
        }
    }

    void addArc(float cx, float cy, float radius, float thickness, float start, float end, Vec4 c)
    {
        if (fabsf(end - start) < 1e-6f) return;

        int seg = max(2, (int)(fabsf(end - start) * 48.0f));

        float inner = radius - thickness * 0.5f;
        float outer = radius + thickness * 0.5f;

        for (int i = 0; i < seg; ++i)
        {
            float a0 = start + (end - start) * (float)i / (float)seg;
            float a1 = start + (end - start) * (float)(i + 1) / (float)seg;

            float ix0 = cx + cosf(a0) * inner;
            float iy0 = cy + sinf(a0) * inner;
            float ox0 = cx + cosf(a0) * outer;
            float oy0 = cy + sinf(a0) * outer;

            float ix1 = cx + cosf(a1) * inner;
            float iy1 = cy + sinf(a1) * inner;
            float ox1 = cx + cosf(a1) * outer;
            float oy1 = cy + sinf(a1) * outer;

            addQuadPoints(ix0, iy0, ox0, oy0, ox1, oy1, ix1, iy1, c);
        }
    }

    void flush()
    {
        if (data.empty()) return;

        glUseProgram(prog.id);
        glUniformMatrix4fv(prog.proj, 1, GL_FALSE, &proj.m[0]);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(float)), data.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(data.size() / 6));
        glBindVertexArray(0);
    }
};

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

static const char* mainVS = R"glsl(
#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vPos;

void main()
{
    vec4 wp = uModel * vec4(aPos, 1.0);
    vPos = wp.xyz;
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uProj * uView * wp;
}
)glsl";

static const char* mainFS = R"glsl(
#version 410 core

in vec3 vNormal;
in vec3 vPos;

uniform vec3 uColor;
uniform float uAlpha;
uniform int uUseLighting;
uniform vec3 uLightDir;
uniform vec3 uCamPos;

out vec4 fragColor;

void main()
{
    vec3 col = uColor;

    if (uUseLighting != 0)
    {
        vec3 N = normalize(vNormal);
        vec3 L = normalize(uLightDir);
        float diff = max(dot(N, L), 0.0);
        float ambient = 0.38;

        col = uColor * (ambient + diff * 0.72);

        vec3 V = normalize(uCamPos - vPos);
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 48.0) * 0.22;
        col += vec3(spec);
    }

    fragColor = vec4(col, uAlpha);
}
)glsl";

static const char* overlayVS = R"glsl(
#version 410 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 uProj;

out vec4 vColor;

void main()
{
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)glsl";

static const char* overlayFS = R"glsl(
#version 410 core

in vec4 vColor;
out vec4 fragColor;

void main()
{
    fragColor = vColor;
}
)glsl";

// ---------------------------------------------------------------------------
// Scene / gameplay constants
// ---------------------------------------------------------------------------

static const float RING_THICKNESS = 0.14f;
static const float DOT_RADIUS = 0.07f;
static const float SPHERE_RADIUS = 0.10f;
static const float PLAYER_Z = 0.17f;
static const float DOOR_Z = 0.075f;
static const float FLOOR_Z = -0.09f;

static const float ROT_STEP = 15.0f * DEG;
static const float ROT_SPEED = ROT_STEP / 0.55f; // radians per second
static const double RANDOM_INTERVAL = 600.0;     // 10 minutes

static const float HALLWAY_RADIUS = 2.55f;
static const float EXIT_RADIUS = 3.10f;
static const float WORLD_LIMIT = 5.50f;

struct Ring
{
    float inner = 0.0f;
    float outer = 0.0f;
    float halfGap = 0.0f;

    float angle = 0.0f;
    float targetAngle = 0.0f;
    bool animating = false;

    float cr = 1.0f, cg = 1.0f, cb = 1.0f;
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static GLFWwindow* window = nullptr;

static Program mainProg;
static Program overlayProg;

static Mesh floorMesh;
static Mesh sphereMesh;
static Mesh ringMesh[3];

static DynamicMesh3D doorMesh;
static Overlay overlay;

static vector<Ring> rings;

static Mat4 proj;
static Mat4 view;

static bool keys[512] = {};
static double mouseXPixels = 0.0;
static double mouseYPixels = 0.0;

static float playerX = 0.0f;
static float playerY = 0.0f;

static bool doorIntervalOpen = false;
static bool doorUsable = false;
static bool anyAnimating = false;
static bool playerInBand = false;
static bool playerInRingZone = false;
static bool playerInHallway = false;
static bool escaped = false;

static float doorCenter = 0.0f;
static float doorHalf = 0.0f;

static double currentTime = 0.0;
static double nextRandom = 0.0;

static float rejectFlash = 0.0f;

// ---------------------------------------------------------------------------
// Geometry builders
// ---------------------------------------------------------------------------

static vector<float> buildFloorMesh()
{
    vector<float> v;

    auto push = [&](float x, float y, float z, float nx, float ny, float nz)
    {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(nx); v.push_back(ny); v.push_back(nz);
    };

    float s = 6.0f;

    push(-s, -s, 0.0f, 0.0f, 0.0f, 1.0f);
    push( s, -s, 0.0f, 0.0f, 0.0f, 1.0f);
    push( s,  s, 0.0f, 0.0f, 0.0f, 1.0f);

    push(-s, -s, 0.0f, 0.0f, 0.0f, 1.0f);
    push( s,  s, 0.0f, 0.0f, 0.0f, 1.0f);
    push(-s,  s, 0.0f, 0.0f, 0.0f, 1.0f);

    return v;
}

static vector<float> buildSphereMesh(float radius, int latBands, int lonBands)
{
    vector<float> v;

    auto push = [&](float theta, float phi)
    {
        float st = sinf(theta);
        float ct = cosf(theta);
        float sp = sinf(phi);
        float cp = cosf(phi);

        float nx = st * cp;
        float ny = st * sp;
        float nz = ct;

        v.push_back(nx * radius);
        v.push_back(ny * radius);
        v.push_back(nz * radius);

        v.push_back(nx);
        v.push_back(ny);
        v.push_back(nz);
    };

    for (int i = 0; i < latBands; ++i)
    {
        float theta0 = PI * (float)i / (float)latBands;
        float theta1 = PI * (float)(i + 1) / (float)latBands;

        for (int j = 0; j < lonBands; ++j)
        {
            float phi0 = 2.0f * PI * (float)j / (float)lonBands;
            float phi1 = 2.0f * PI * (float)(j + 1) / (float)lonBands;

            push(theta0, phi0);
            push(theta1, phi0);
            push(theta1, phi1);

            push(theta0, phi0);
            push(theta1, phi1);
            push(theta0, phi1);
        }
    }

    return v;
}

static vector<float> buildRingMesh(float inner, float outer, float halfGap, float thickness)
{
    vector<float> v;

    auto push = [&](float x, float y, float z, float nx, float ny, float nz)
    {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(nx); v.push_back(ny); v.push_back(nz);
    };

    const int seg = 112;
    const float start = halfGap;
    const float end = 2.0f * PI - halfGap;

    const float zt = thickness * 0.5f;
    const float zb = -thickness * 0.5f;

    for (int i = 0; i < seg; ++i)
    {
        float a0 = start + (end - start) * (float)i / (float)seg;
        float a1 = start + (end - start) * (float)(i + 1) / (float)seg;

        float c0 = cosf(a0), s0 = sinf(a0);
        float c1 = cosf(a1), s1 = sinf(a1);

        // Top face
        push(inner * c0, inner * s0, zt, 0.0f, 0.0f, 1.0f);
        push(outer * c0, outer * s0, zt, 0.0f, 0.0f, 1.0f);
        push(outer * c1, outer * s1, zt, 0.0f, 0.0f, 1.0f);

        push(inner * c0, inner * s0, zt, 0.0f, 0.0f, 1.0f);
        push(outer * c1, outer * s1, zt, 0.0f, 0.0f, 1.0f);
        push(inner * c1, inner * s1, zt, 0.0f, 0.0f, 1.0f);

        // Bottom face
        push(inner * c0, inner * s0, zb, 0.0f, 0.0f, -1.0f);
        push(outer * c1, outer * s1, zb, 0.0f, 0.0f, -1.0f);
        push(outer * c0, outer * s0, zb, 0.0f, 0.0f, -1.0f);

        push(inner * c0, inner * s0, zb, 0.0f, 0.0f, -1.0f);
        push(inner * c1, inner * s1, zb, 0.0f, 0.0f, -1.0f);
        push(outer * c1, outer * s1, zb, 0.0f, 0.0f, -1.0f);

        // Outer cylindrical side
        push(outer * c0, outer * s0, zt, c0, s0, 0.0f);
        push(outer * c0, outer * s0, zb, c0, s0, 0.0f);
        push(outer * c1, outer * s1, zb, c1, s1, 0.0f);

        push(outer * c0, outer * s0, zt, c0, s0, 0.0f);
        push(outer * c1, outer * s1, zb, c1, s1, 0.0f);
        push(outer * c1, outer * s1, zt, c1, s1, 0.0f);

        // Inner cylindrical side
        push(inner * c0, inner * s0, zt, -c0, -s0, 0.0f);
        push(inner * c1, inner * s1, zt, -c1, -s1, 0.0f);
        push(inner * c1, inner * s1, zb, -c1, -s1, 0.0f);

        push(inner * c0, inner * s0, zt, -c0, -s0, 0.0f);
        push(inner * c1, inner * s1, zb, -c1, -s1, 0.0f);
        push(inner * c0, inner * s0, zb, -c0, -s0, 0.0f);
    }

    auto addCap = [&](float a, float nx, float ny)
    {
        float c = cosf(a);
        float s = sinf(a);

        push(inner * c, inner * s, zt, nx, ny, 0.0f);
        push(outer * c, outer * s, zt, nx, ny, 0.0f);
        push(outer * c, outer * s, zb, nx, ny, 0.0f);

        push(inner * c, inner * s, zt, nx, ny, 0.0f);
        push(outer * c, outer * s, zb, nx, ny, 0.0f);
        push(inner * c, inner * s, zb, nx, ny, 0.0f);
    };

    // Gap caps
    float cs = cosf(start), ss = sinf(start);
    float ce = cosf(end), se = sinf(end);

    addCap(start, ss, -cs);
    addCap(end, -se, ce);

    return v;
}

// ---------------------------------------------------------------------------
// Door interval logic
// ---------------------------------------------------------------------------

static bool pointInAllGaps(float angle, const vector<Ring>& rs)
{
    for (const Ring& r : rs)
    {
        float d = fabsf(normSigned(angle - r.angle));
        if (d > r.halfGap + 1e-4f)
            return false;
    }
    return true;
}

static bool computeDoorInterval(const vector<Ring>& rs, float& centerOut, float& halfOut)
{
    vector<float> candidates;
    candidates.push_back(0.0f);

    for (const Ring& r : rs)
    {
        candidates.push_back(norm01(r.angle - r.halfGap));
        candidates.push_back(norm01(r.angle + r.halfGap));
    }

    float ref = 0.0f;
    bool found = false;

    for (float c : candidates)
    {
        if (pointInAllGaps(c, rs))
        {
            ref = c;
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    float lo = -PI;
    float hi = PI;

    for (const Ring& r : rs)
    {
        float d = normSigned(r.angle - ref);
        float l = d - r.halfGap;
        float rr = d + r.halfGap;

        lo = max(lo, l);
        hi = min(hi, rr);
    }

    if (hi < lo - 1e-4f)
        return false;

    float centerRel = (lo + hi) * 0.5f;
    halfOut = (hi - lo) * 0.5f;
    centerOut = norm01(ref + centerRel);

    return halfOut > 1e-4f;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

static void initRings()
{
    rings.clear();

    Ring a;
    a.inner = 1.00f;
    a.outer = 1.35f;
    a.halfGap = 40.0f * DEG;
    a.angle = 0.0f;
    a.targetAngle = 0.0f;
    a.animating = false;
    a.cr = 0.86f; a.cg = 0.31f; a.cb = 0.20f;
    rings.push_back(a);

    Ring b;
    b.inner = 1.45f;
    b.outer = 1.80f;
    b.halfGap = 40.0f * DEG;
    b.angle = 50.0f * DEG;
    b.targetAngle = 50.0f * DEG;
    b.animating = false;
    b.cr = 0.18f; b.cg = 0.55f; b.cb = 0.64f;
    rings.push_back(b);

    Ring c;
    c.inner = 1.90f;
    c.outer = 2.25f;
    c.halfGap = 40.0f * DEG;
    c.angle = 100.0f * DEG;
    c.targetAngle = 100.0f * DEG;
    c.animating = false;
    c.cr = 0.96f; c.cg = 0.65f; c.cb = 0.18f;
    rings.push_back(c);
}

// ---------------------------------------------------------------------------
// State updates
// ---------------------------------------------------------------------------

static void updateStates()
{
    anyAnimating = false;
    for (const Ring& r : rings)
        if (r.animating)
            anyAnimating = true;

    float rr = hypotf(playerX, playerY);

    playerInHallway = (rr >= HALLWAY_RADIUS);
    escaped = (rr >= EXIT_RADIUS);

    // Inside the container/ring stack: cannot rotate rings.
    // This prevents the dot from being trapped between rings.
    playerInRingZone = (rr > rings.front().inner - DOT_RADIUS && rr < HALLWAY_RADIUS);

    playerInBand = false;
    for (const Ring& r : rings)
    {
        if (rr > r.inner - DOT_RADIUS && rr < r.outer + DOT_RADIUS)
        {
            playerInBand = true;
            break;
        }
    }

    doorIntervalOpen = computeDoorInterval(rings, doorCenter, doorHalf);
    doorUsable = doorIntervalOpen && !anyAnimating && doorHalf > 0.03f;
}

static void updateRings(float dt)
{
    for (Ring& r : rings)
    {
        float diff = normSigned(r.targetAngle - r.angle);

        if (fabsf(diff) > 1e-5f)
        {
            r.animating = true;

            float step = ROT_SPEED * dt;

            if (fabsf(diff) <= step)
            {
                r.angle = norm01(r.targetAngle);
                r.animating = false;
            }
            else
            {
                float dir = (diff > 0.0f) ? 1.0f : -1.0f;
                r.angle = norm01(r.angle + dir * step);
            }
        }
        else
        {
            r.angle = norm01(r.targetAngle);
            r.animating = false;
        }
    }
}

static bool attemptRotate(int ringIndex, int dir)
{
    if (ringIndex < 0 || ringIndex >= (int)rings.size())
        return false;

    // Do not rotate while dot is in hallway or anywhere inside the ring zone.
    if (playerInHallway || playerInRingZone || anyAnimating)
        return false;

    rings[ringIndex].targetAngle = norm01(rings[ringIndex].targetAngle + dir * ROT_STEP);
    return true;
}

static bool attemptRandom(bool /*forced*/)
{
    if (playerInHallway)
        return false;

    if (playerInRingZone || anyAnimating)
        return false;

    int idx = rand() % (int)rings.size();
    int dir = (rand() % 2) ? 1 : -1;

    return attemptRotate(idx, dir);
}

static void updateRandom(double now)
{
    if (now >= nextRandom)
    {
        if (attemptRandom(false))
            nextRandom = now + RANDOM_INTERVAL;
        else
            nextRandom = now + 1.0; // retry soon if blocked
    }
}

// ---------------------------------------------------------------------------
// Collision / movement
// ---------------------------------------------------------------------------

static bool isValidPosition(float x, float y)
{
    float rr = hypotf(x, y);

    if (rr > WORLD_LIMIT)
        return false;

    for (const Ring& r : rings)
    {
        bool inBand = (rr > r.inner - DOT_RADIUS && rr < r.outer + DOT_RADIUS);

        if (inBand)
        {
            // The dot may only occupy a rotating ring band when the global doorway is open.
            if (!doorUsable)
                return false;

            float ang = atan2f(y, x);
            float rel = fabsf(normSigned(ang - doorCenter));

            // Approximate angular radius occupied by the dot at this radius.
            float margin = DOT_RADIUS / max(rr, 0.001f);
            float allowed = doorHalf - margin;

            if (allowed < 0.0f)
                return false;

            if (rel > allowed)
                return false;
        }
    }

    return true;
}

static void movePlayer(float dt)
{
    float vx = 0.0f;
    float vy = 0.0f;

    if (keys[GLFW_KEY_LEFT] || keys[GLFW_KEY_A]) vx -= 1.0f;
    if (keys[GLFW_KEY_RIGHT] || keys[GLFW_KEY_D]) vx += 1.0f;
    if (keys[GLFW_KEY_UP] || keys[GLFW_KEY_W]) vy += 1.0f;
    if (keys[GLFW_KEY_DOWN] || keys[GLFW_KEY_S]) vy -= 1.0f;

    float len = hypotf(vx, vy);
    if (len <= 1e-6f)
        return;

    vx /= len;
    vy /= len;

    const float speed = 1.9f;

    float nx = playerX + vx * speed * dt;
    if (isValidPosition(nx, playerY))
        playerX = nx;

    float ny = playerY + vy * speed * dt;
    if (isValidPosition(playerX, ny))
        playerY = ny;
}

// ---------------------------------------------------------------------------
// GUI buttons
// ---------------------------------------------------------------------------

struct Button
{
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    int ring = 0;
    int dir = 0; // -1 CCW, +1 CW
};

static vector<Button> makeButtons(int fbW, int fbH)
{
    vector<Button> b;
    b.reserve(6);

    const float bw = 66.0f;
    const float bh = 56.0f;
    const float gap = 14.0f;

    float total = 6.0f * bw + 5.0f * gap;
    float startX = ((float)fbW - total) * 0.5f;
    float y = (float)fbH - bh - 28.0f;

    for (int i = 0; i < 3; ++i)
    {
        for (int k = 0; k < 2; ++k)
        {
            Button btn;
            btn.ring = i;
            btn.dir = (k == 0) ? -1 : 1;
            btn.w = bw;
            btn.h = bh;
            btn.x = startX + (float)(i * 2 + k) * (bw + gap);
            btn.y = y;
            b.push_back(btn);
        }
    }

    return b;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

static void drawMainMesh(const Mesh& mesh, const Mat4& model, Vec3 color, float alpha, bool lighting)
{
    glUseProgram(mainProg.id);
    glUniformMatrix4fv(mainProg.model, 1, GL_FALSE, &model.m[0]);
    glUniform3fv(mainProg.color, 1, &color.x);
    glUniform1f(mainProg.alpha, alpha);
    glUniform1i(mainProg.useLighting, lighting ? 1 : 0);

    mesh.draw();
}

static void drawUI(int fbW, int fbH, double now)
{
    vector<Button> btns = makeButtons(fbW, fbH);

    for (const Button& b : btns)
    {
        bool hover =
            mouseXPPixels >= b.x && mouseXPPixels < b.x + b.w &&
            mouseYPixels >= b.y && mouseYPixels < b.y + b.h;

        Vec4 bg;
        bg.x = 0.12f; bg.y = 0.13f; bg.z = 0.16f; bg.w = hover ? 0.96f : 0.86f;
        overlay.addQuad(b.x, b.y, b.x + b.w, b.y + b.h, bg);

        Vec4 border;
        border.x = rings[b.ring].cr;
        border.y = rings[b.ring].cg;
        border.z = rings[b.ring].cb;
        border.w = 1.0f;

        if (rejectFlash > 0.0f)
        {
            border.x = 1.0f;
            border.y = 0.25f;
            border.z = 0.25f;
            border.w = 1.0f;
        }

        float t = 3.0f;

        // Top
        overlay.addQuad(b.x, b.y, b.x + b.w, b.y + t, border);
        // Bottom
        overlay.addQuad(b.x, b.y + b.h - t, b.x + b.w, b.y + b.h, border);
        // Left
        overlay.addQuad(b.x, b.y, b.x + t, b.y + b.h, border);
        // Right
        overlay.addQuad(b.x + b.w - t, b.y, b.x + b.w, b.y + b.h, border);

        float cx = b.x + b.w * 0.5f;
        float cy = b.y + b.h * 0.5f;
        float s = 13.0f;

        Vec4 arrow;
        arrow.x = 1.0f; arrow.y = 1.0f; arrow.z = 1.0f; arrow.w = 0.95f;

        if (b.dir < 0)
        {
            // Left / CCW arrow
            overlay.addTriangle(cx + s, cy - s,
                                cx - s, cy,
                                cx + s, cy + s,
                                arrow);
        }
        else
        {
            // Right / CW arrow
            overlay.addTriangle(cx - s, cy - s,
                                cx + s, cy,
                                cx - s, cy + s,
                                arrow);
        }
    }

    // Door state indicator, top-left
    {
        float cx = 48.0f;
        float cy = 48.0f;
        float r = 24.0f;

        Vec4 doorCol;
        if (doorUsable)
        {
            doorCol.x = 0.20f; doorCol.y = 1.00f; doorCol.z = 0.45f; doorCol.w = 1.0f;
        }
        else
        {
            doorCol.x = 1.00f; doorCol.y = 0.25f; doorCol.z = 0.25f; doorCol.w = 1.0f;
        }

        Vec4 fill = doorCol;
        fill.w = 0.22f;
        overlay.addCircle(cx, cy, r - 5.0f, 48, fill);
        overlay.addArc(cx, cy, r, 4.0f, 0.0f, 2.0f * PI, doorCol);
    }

    // Random rotation countdown, top-right
    {
        float cx = (float)fbW - 56.0f;
        float cy = 56.0f;
        float r = 28.0f;

        double remaining = nextRandom - now;
        if (remaining < 0.0) remaining = 0.0;

        float progress = 1.0f - (float)(remaining / RANDOM_INTERVAL);
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        Vec4 track;
        track.x = 0.25f; track.y = 0.27f; track.z = 0.30f; track.w = 0.90f;
        overlay.addArc(cx, cy, r, 6.0f, 0.0f, 2.0f * PI, track);

        Vec4 progCol;
        if (playerInHallway)
        {
            progCol.x = 0.45f; progCol.y = 0.45f; progCol.z = 0.45f; progCol.w = 1.0f;
        }
        else if (anyAnimating || playerInRingZone)
        {
            progCol.x = 1.00f; progCol.y = 0.80f; progCol.z = 0.20f; progCol.w = 1.0f;
        }
        else
        {
            progCol.x = 0.25f; progCol.y = 0.85f; progCol.z = 1.00f; progCol.w = 1.0f;
        }

        float start = -PI * 0.5f;
        overlay.addArc(cx, cy, r, 6.0f, start, start + 2.0f * PI * progress, progCol);
    }

    // Simple hallway/escape tint
    if (playerInHallway)
    {
        Vec4 bar;
        bar.x = 0.20f; bar.y = 1.00f; bar.z = 0.45f; bar.w = 0.22f;
        overlay.addQuad((float)fbW * 0.5f - 90.0f, 18.0f, (float)fbW * 0.5f + 90.0f, 34.0f, bar);
    }
}

static void render(double now)
{
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    if (fbW <= 0 || fbH <= 0)
        return;

    glViewport(0, 0, fbW, fbH);
    glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float aspect = (float)fbW / (float)fbH;
    proj = perspective(45.0f * DEG, aspect, 0.1f, 100.0f);
    view = lookAt(Vec3{0.0f, 0.0f, 7.2f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

    glUseProgram(mainProg.id);
    glUniformMatrix4fv(mainProg.proj, 1, GL_FALSE, &proj.m[0]);
    glUniformMatrix4fv(mainProg.view, 1, GL_FALSE, &view.m[0]);

    Vec3 light = normalizeVec(Vec3{0.35f, 0.45f, 0.85f});
    glUniform3fv(mainProg.lightDir, 1, &light.x);

    Vec3 cam{0.0f, 0.0f, 7.2f};
    glUniform3fv(mainProg.camPos, 1, &cam.x);

    // Floor
    drawMainMesh(floorMesh, translate(0.0f, 0.0f, FLOOR_Z), Vec3{0.13f, 0.14f, 0.16f}, 1.0f, true);

    // Rings
    for (int i = 0; i < (int)rings.size(); ++i)
    {
        Vec3 c{rings[i].cr, rings[i].cg, rings[i].cb};
        drawMainMesh(ringMesh[i], rotateZ(rings[i].angle), c, 1.0f, true);
    }

    // Person dot
    Vec3 dotCol;
    if (escaped)
        dotCol = Vec3{0.25f, 1.00f, 0.45f};
    else
        dotCol = Vec3{1.00f, 0.86f, 0.25f};

    drawMainMesh(sphereMesh, translate(playerX, playerY, PLAYER_Z), dotCol, 1.0f, true);

    // Transparent doorway corridor when open
    doorMesh.begin();

    if (doorUsable)
    {
        float a0 = doorCenter - doorHalf * 0.90f;
        float a1 = doorCenter + doorHalf * 0.90f;
        int seg = 48;
        float R = EXIT_RADIUS + 0.25f;

        for (int i = 0; i < seg; ++i)
        {
            float ang0 = a0 + (a1 - a0) * (float)i / (float)seg;
            float ang1 = a0 + (a1 - a0) * (float)(i + 1) / (float)seg;

            doorMesh.add(0.0f, 0.0f, DOOR_Z, 0.0f, 0.0f, 1.0f);
            doorMesh.add(cosf(ang0) * R, sinf(ang0) * R, DOOR_Z, 0.0f, 0.0f, 1.0f);
            doorMesh.add(cosf(ang1) * R, sinf(ang1) * R, DOOR_Z, 0.0f, 0.0f, 1.0f);
        }
    }

    glDepthMask(GL_FALSE);
    doorMesh.draw(mainProg, identity(), Vec3{0.20f, 1.00f, 0.45f}, 0.16f, false);
    glDepthMask(GL_TRUE);

    // 2D overlay GUI
    glDisable(GL_DEPTH_TEST);

    overlay.clear();
    Mat4 ortho = ortho2D(0.0f, (float)fbW, (float)fbH, 0.0f, -1.0f, 1.0f);
    overlay.setProjection(ortho);

    drawUI(fbW, fbH, now);

    overlay.flush();

    glEnable(GL_DEPTH_TEST);
}

// ---------------------------------------------------------------------------
// GLFW callbacks
// ---------------------------------------------------------------------------

static void keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(w, GLFW_TRUE);

    if (key >= 0 && key < 512)
    {
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
            keys[key] = true;
        else if (action == GLFW_RELEASE)
            keys[key] = false;
    }

    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_R)
        {
            if (attemptRandom(true))
                nextRandom = currentTime + RANDOM_INTERVAL;
            else
                rejectFlash = 0.35f;
        }
    }
}

static void cursorCallback(GLFWwindow* w, double xpos, double ypos)
{
    int fbW = 0, fbH = 0;
    int winW = 0, winH = 0;

    glfwGetFramebufferSize(w, &fbW, &fbH);
    glfwGetWindowSize(w, &winW, &winH);

    if (winW <= 0 || winH <= 0)
        return;

    mouseXPixels = xpos * (double)fbW / (double)winW;
    mouseYPixels = ypos * (double)fbH / (double)winH;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(w, &fbW, &fbH);

    vector<Button> btns = makeButtons(fbW, fbH);

    for (const Button& b : btns)
    {
        bool hit =
            mouseXPixels >= b.x && mouseXPixels < b.x + b.w &&
            mouseYPixels >= b.y && mouseYPixels < b.y + b.h;

        if (hit)
        {
            if (!attemptRotate(b.ring, b.dir))
                rejectFlash = 0.35f;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window = glfwCreateWindow(1280, 720, "OCP Three-Ring Escape", nullptr, nullptr);
    if (!window)
    {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW: %s\n", glewGetErrorString(glewErr));
        glfwTerminate();
        return 1;
    }

    // Clear errors generated by some GLEW versions.
    while (glGetError() != GL_NO_ERROR) {}

    mainProg = makeProgram(mainVS, mainFS, true);
    overlayProg = makeProgram(overlayVS, overlayFS, false);

    initRings();

    floorMesh.create(buildFloorMesh());
    sphereMesh.create(buildSphereMesh(SPHERE_RADIUS, 16, 24));

    for (int i = 0; i < (int)rings.size(); ++i)
    {
        ringMesh[i].create(buildRingMesh(rings[i].inner, rings[i].outer, rings[i].halfGap, RING_THICKNESS));
    }

    doorMesh.init();
    overlay.init(overlayProg);

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    srand((unsigned)time(nullptr));

    double start = glfwGetTime();
    currentTime = start;
    nextRandom = start + RANDOM_INTERVAL;

    updateStates();

    double last = start;
    double accumulator = 0.0;
    const double physicsDt = 1.0 / 120.0;

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        double dt = now - last;
        last = now;

        if (dt > 0.1) dt = 0.1;

        currentTime = now;

        if (rejectFlash > 0.0f)
            rejectFlash -= (float)dt;

        updateRings((float)dt);
        updateStates();

        updateRandom(now);
        updateStates();

        accumulator += dt;
        while (accumulator >= physicsDt)
        {
            movePlayer((float)physicsDt);
            accumulator -= physicsDt;
        }

        updateStates();

        render(now);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Optional cleanup omitted for brevity.
    glfwTerminate();
    return 0;
}