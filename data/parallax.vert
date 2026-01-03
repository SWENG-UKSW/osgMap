varying vec2 texCoord;
varying vec3 viewDir;

void main() {
    texCoord = gl_Vertex.xy * 0.05;
    vec4 eyePos = gl_ModelViewMatrix * gl_Vertex;
    viewDir = normalize(eyePos.xyz);
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}