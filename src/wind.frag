uniform sampler2D baseTexture;
varying vec2 texCoord;

void main() {
    gl_FragColor = texture2D(baseTexture, texCoord);
}