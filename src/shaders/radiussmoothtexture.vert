#version 440

layout(location = 0) in vec4 vertex;
layout(location = 1) in vec2 multiTexCoord;
layout(location = 2) in float vertexOpacityIn;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out float vertexOpacity;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float opacity;
    vec2 pixelSize;
} ubuf;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vec4 pos = ubuf.qt_Matrix * vertex;
    gl_Position = pos;
    texCoord = multiTexCoord;
    vertexOpacity = ubuf.opacity * vertexOpacityIn;
}
