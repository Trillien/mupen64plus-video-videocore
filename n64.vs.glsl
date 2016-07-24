// mupen64plus-video-videocore/n64.vs.glsl

attribute vec4 aPosition;
attribute vec2 aTextureUv;
attribute vec4 aTexture0Bounds;
attribute vec4 aTexture1Bounds;
attribute vec4 aShade;
attribute vec4 aPrimitive;
attribute vec4 aEnvironment;
attribute vec3 aControl;

varying vec2 vTextureUv;
varying vec4 vTexture0Bounds;
varying vec4 vTexture1Bounds;
varying vec4 vShade;
varying vec4 vPrimitive;
varying vec4 vEnvironment;
varying vec3 vControl;

void main(void) {
    if (aTexture0Bounds.z != 0.0 && aTexture0Bounds.w != 0.0)
        vTextureUv = aTextureUv / abs(aTexture0Bounds.zw);  // FIXME(tachi)
    else
        vTextureUv = aTextureUv;
    vTexture0Bounds = aTexture0Bounds / 1024.0;
    vTexture1Bounds = aTexture1Bounds / 1024.0;
    vShade = aShade;
    vPrimitive = aPrimitive;
    vEnvironment = aEnvironment;
    vControl = aControl;
    gl_Position = aPosition;
}

