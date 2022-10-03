/* $Id: cconvAYUV.c $ */
#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect uSrcTex;
void vpoxCConvApplyAYUV(vec4 color);
void vpoxCConv()
{
    vec2 srcCoord = vec2(gl_TexCoord[0]);
    vec4 color = texture2DRect(uSrcTex, srcCoord);
    vpoxCConvApplyAYUV(color);
}
