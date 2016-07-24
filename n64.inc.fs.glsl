// mupen64plus-video-videocore/n64.inc.fs.glsl

uniform sampler2D uTexture0;
uniform sampler2D uTexture1;

varying vec2 vTextureUv;
varying vec4 vTexture0Bounds;
varying vec4 vTexture1Bounds;
varying vec4 vShade;
varying vec4 vPrimitive;
varying vec4 vEnvironment;
varying vec3 vControl;

vec2 AtlasUv(vec4 textureBounds) {
    vec2 uv = vTextureUv;
    if (textureBounds.z > 0.0)
        uv.x = clamp(uv.x, 0.0, 1.0);
    else if (uv.x < 0.0)
        uv.x = 1.0 - fract(uv.x);
    else
        uv.x = fract(uv.x);

    if (textureBounds.w > 0.0)
        uv.y = clamp(uv.y, 0.0, 1.0);
    else if (uv.y < 0.0)
        uv.y = 1.0 - fract(uv.y);
    else
        uv.y = fract(uv.y);

    return uv * abs(textureBounds.zw) + textureBounds.xy;
}

