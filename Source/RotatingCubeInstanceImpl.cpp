#include "PluginEditor.h"
#include <cmath>
#include <array>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct Vec3 { float x, y, z; };

static Vec3 rotateX(Vec3 v, float a)
{
    float c = std::cos(a), s = std::sin(a);
    return { v.x, v.y * c - v.z * s, v.y * s + v.z * c };
}
static Vec3 rotateY(Vec3 v, float a)
{
    float c = std::cos(a), s = std::sin(a);
    return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}
static Vec3 rotateZ(Vec3 v, float a)
{
    float c = std::cos(a), s = std::sin(a);
    return { v.x * c - v.y * s, v.x * s + v.y * c, v.z };
}

static juce::Point<float> project(Vec3 v, float cx, float cy, float fov, float camZ)
{
    float denom = v.z + camZ;
    if (denom < 0.01f) denom = 0.01f;
    float px = (v.x / denom) * fov + cx;
    float py = (v.y / denom) * fov + cy;
    return { px, py };
}

static Vec3 cross(Vec3 a, Vec3 b)
{
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}
static float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

// ---------------------------------------------------------------------------
// Cube geometry
// ---------------------------------------------------------------------------

// 8 vertices of a unit cube centred at origin
static const Vec3 BASE_VERTS[8] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},   // front  (z = -1)
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1}    // back   (z =  1)
};

// 6 faces, each defined by 4 vertex indices (winding = outward normal viewable)
static const int FACES[6][4] = {
    {0, 1, 2, 3},   // front
    {5, 4, 7, 6},   // back
    {4, 0, 3, 7},   // left
    {1, 5, 6, 2},   // right
    {3, 2, 6, 7},   // top
    {4, 5, 1, 0}    // bottom
};

// Outward normals for each face (un-rotated)
static const Vec3 FACE_NORMALS[6] = {
    { 0,  0, -1},   // front
    { 0,  0,  1},   // back
    {-1,  0,  0},   // left
    { 1,  0,  0},   // right
    { 0,  1,  0},   // top
    { 0, -1,  0}    // bottom
};

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void AudioVisualizerEditor::RotatingCubeInstance::update(float value)
{
    // Base rotation speed + audio boost
    float boost = 1.0f + value * 5.0f;

    rotX += speedX * 0.012f * boost;
    rotY += speedY * 0.018f * boost;
    rotZ += speedZ * 0.007f * boost;

    // Scale pulses with audio (quick attack, slow release)
    float targetScale = 1.0f + value * 0.35f;
    scale = scale * 0.85f + targetScale * 0.15f;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void AudioVisualizerEditor::RotatingCubeInstance::draw(
    juce::Graphics& g,
    const juce::Rectangle<int>& bounds,
    bool lightMode,
    juce::Colour cubeColor)
{
    float cx = bounds.getX() + bounds.getWidth()  * 0.5f;
    float cy = bounds.getY() + bounds.getHeight() * 0.5f;

    // fov scales with the smaller dimension so it fills the panel nicely
    float minDim = std::min(bounds.getWidth(), bounds.getHeight());
    float fov    = minDim * 0.38f * scale;
    float camZ   = 4.0f;   // camera distance along +Z

    // Light direction (world space, normalised)
    Vec3 light = { 0.6f, -0.8f, -0.5f };
    float lightLen = std::sqrt(dot(light, light));
    light = { light.x / lightLen, light.y / lightLen, light.z / lightLen };

    // -----------------------------------------------------------------------
    // Transform all 8 vertices
    // -----------------------------------------------------------------------
    Vec3 tv[8];
    for (int i = 0; i < 8; i++)
    {
        Vec3 v = BASE_VERTS[i];
        v = rotateX(v, rotX);
        v = rotateY(v, rotY);
        v = rotateZ(v, rotZ);
        tv[i] = v;
    }

    // -----------------------------------------------------------------------
    // Build face list with depth + lighting
    // -----------------------------------------------------------------------
    struct FaceInfo {
        int idx;
        float depth;     // average Z of four corners
        float shade;     // 0..1 brightness from lighting
        bool  visible;
    };

    FaceInfo faceInfos[6];
    for (int fi = 0; fi < 6; fi++)
    {
        const int* qi = FACES[fi];

        // Average Z for painter's sort
        float avgZ = (tv[qi[0]].z + tv[qi[1]].z + tv[qi[2]].z + tv[qi[3]].z) * 0.25f;

        // Rotate the face normal the same way
        Vec3 n = FACE_NORMALS[fi];
        n = rotateX(n, rotX);
        n = rotateY(n, rotY);
        n = rotateZ(n, rotZ);

        // Back-face cull: normal must face camera (-Z direction)
        float facing = -n.z;   // positive = faces camera
        bool visible = facing > 0.0f;

        // Diffuse lighting
        float diffuse = juce::jlimit(0.0f, 1.0f, dot(n, { -light.x, -light.y, -light.z }));
        float shade = 0.25f + 0.75f * diffuse;  // ambient + diffuse

        faceInfos[fi] = { fi, avgZ, shade, visible };
    }

    // Sort back-to-front (highest Z drawn first)
    std::sort(std::begin(faceInfos), std::end(faceInfos),
              [](const FaceInfo& a, const FaceInfo& b){ return a.depth > b.depth; });

    // -----------------------------------------------------------------------
    // Draw faces
    // -----------------------------------------------------------------------
    for (const auto& fi : faceInfos)
    {
        if (!fi.visible) continue;

        const int* qi = FACES[fi.idx];

        // Project four corners
        juce::Point<float> pts[4];
        for (int k = 0; k < 4; k++)
            pts[k] = project(tv[qi[k]], cx, cy, fov, camZ);

        // Build filled quad path
        juce::Path facePath;
        facePath.startNewSubPath(pts[0]);
        facePath.lineTo(pts[1]);
        facePath.lineTo(pts[2]);
        facePath.lineTo(pts[3]);
        facePath.closeSubPath();

        // Face fill – colour shaded by lighting + slight depth darkening
        juce::Colour faceColor = cubeColor
            .withMultipliedBrightness(fi.shade)
            .withAlpha(0.82f);

        g.setColour(faceColor);
        g.fillPath(facePath);

        // Edge lines – rounded joins/caps prevent corner spikes
        juce::Colour edgeColor = cubeColor.brighter(0.5f).withAlpha(0.9f);
        g.setColour(edgeColor);
        g.strokePath(facePath, juce::PathStrokeType(1.2f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));

    }
}
