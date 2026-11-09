//! Pure parts of `hpl::cGpuShaderManager`: HPSL file aliases, source patches and sampler bindings.
//! The patch passes mirror the `std::regex` ECMAScript patterns of the C++ with hand-written
//! matchers (no regex crate); every pattern is deterministic, so no backtracking is needed.

use crate::string::{replace_string_to, set_file_ext, split, token_to_int};

type Bytes = Vec<u8>;

const ALIASES: [[&str; 2]; 5] = [
    [
        "deferred_illumination_frag.glsl",
        "deferred_illumination_solid_frag.hpsl",
    ],
    [
        "deferred_gbuffer_skybox_frag.glsl",
        "deferred_skybox_frag.hpsl",
    ],
    [
        "deferred_decal_frag.glsl",
        "deferred_gbuffer_decal_frag.hpsl",
    ],
    ["gui_vtx.glsl", "base_vtx.hpsl"],
    ["gui_frag.glsl", "base_frag.hpsl"],
];

const PATCHES: [[&str; 3]; 4] = [
    [
        "deferred_light_frag.hpsl",
        "//Translucency\nuniform float afTranslucencyScale;",
        "@ifdef BoxMask\n\tuniform cMatrixf a_mtxInvView;\n\tuniform cVector3f avMaskCenter;\n\tuniform cVector3f avMaskExtent;\n@endif\nuniform float afSpotNearClip;\n",
    ],
    [
        "deferred_light_frag.hpsl",
        "\tvDiffuse *= fAttenuatuion;",
        concat!(
            "\t@ifdef BoxMask\n\t\tcVector3f vMaskDelta = abs((mul(a_mtxInvView, cVector4f(vPos, 1)).xyz - avMaskCenter) * 2.0 / avMaskExtent);\n",
            "\t\tvDiffuse *= step(max(max(vMaskDelta.x, vMaskDelta.y), vMaskDelta.z), 1.0);\n\t@endif\n"
        ),
    ],
    [
        "deferred_transparent_frag.hpsl",
        "\t\tcVector2f avInvScreenSize;\n\t}",
        "\t\tcMatrixf px_mtxLightProbe;\n\t\tfloat afInvFarPlane;\n\t\tcVector4f avFadeProp;\n",
    ],
    [
        "water_surface_frag.hpsl",
        "\t};\n@endif\n\n////////////////////////////////\n// samplers",
        concat!(
            "\t\tint mlBlendMode;\n\t\tfloat mfFadeWhenNear;\n\t\tcVector3f avFogCameraPos;\n\t\tcVector4f avFogSettings;\n",
            "\t\tcVector4f avSecondFogSettings;\n\t\tint alFogType;\n\t\tcVector4f avReflectionMul;\n"
        ),
    ],
];

const REPLACEMENTS: [[&str; 3]; 26] = [
    [
        "posteffect_tonemapping_frag.hpsl",
        "uniform float afKey;\n",
        "uniform float afKey;\n@ifdef UseHdrOutput\nuniform float afHdrHeadroom;\n@endif\n",
    ],
    [
        "posteffect_tonemapping_frag.hpsl",
        "\tvSourceColor = clamp(vSourceColor, cVector4f(0.0, 0.0, 0.0, 0.0), cVector4f(1.0, 1.0, 1.0, 1.0));\n",
        "@ifdef UseHdrOutput\n\tcVector3f vHdrExcess = max(vSourceColor.rgb - 1.0, 0.0);\n@endif\n\tvSourceColor = clamp(vSourceColor, cVector4f(0.0, 0.0, 0.0, 0.0), cVector4f(1.0, 1.0, 1.0, 1.0));\n",
    ],
    [
        "posteffect_tonemapping_frag.hpsl",
        "vSourceColor = pow(vSourceColor, cVector4f(afInvGammaCorrection));\n\t@endif\n",
        "vSourceColor = pow(vSourceColor, cVector4f(afInvGammaCorrection));\n\t@endif\n@ifdef UseHdrOutput\n\tcVector3f vHdrExcess = max(vSourceColor.rgb - 1.0, 0.0);\n@endif\n",
    ],
    [
        "posteffect_tonemapping_frag.hpsl",
        "\tout_vColor0 = vSourceColor;",
        "@ifdef UseHdrOutput\n\tvSourceColor.rgb = min(vSourceColor.rgb, 1.0) + (afHdrHeadroom - 1.0) * (1.0 - 2.0 / (exp(2.0 * vHdrExcess / (afHdrHeadroom - 1.0)) + 1.0));\n@endif\n\tout_vColor0 = vSourceColor;",
    ],
    [
        "posteffect_tonemapping_frag.hpsl",
        "\tout_vColor0 = cVector4f(vSourceColor.rgb, dot(",
        "@ifdef UseHdrOutput\n\tvSourceColor.rgb = min(vSourceColor.rgb, 1.0) + (afHdrHeadroom - 1.0) * (1.0 - 2.0 / (exp(2.0 * vHdrExcess / (afHdrHeadroom - 1.0)) + 1.0));\n@endif\n\tout_vColor0 = cVector4f(vSourceColor.rgb, dot(",
    ],
    [
        "deferred_light_frag.hpsl",
        "cVector4f(afSliceDiv, 1.0, 1.0, 1.0)",
        "cVector4f(0.5, 0.5, 1.0, 1.0)",
    ],
    [
        "deferred_light_frag.hpsl",
        "cVector4f(lSlice * afSliceDiv, 0.0, 0.0, 0.0)",
        "cVector4f(mod(float(lSlice), 2.0) * 0.5, floor(float(lSlice) * 0.5) * 0.5, 0.0, 0.0)",
    ],
    [
        "deferred_light_frag.hpsl",
        "cVector4f((lSlice + 1) * afSliceDiv, 0.0, 0.0, 0.0)",
        "cVector4f(mod(float(lSlice + 1), 2.0) * 0.5, floor(float(lSlice + 1) * 0.5) * 0.5, 0.0, 0.0)",
    ],
    [
        "deferred_gbuffer_solid_frag.hpsl",
        "@ifdef UseLiquid\n\t\t  in cVector4f px_vLiquidFade,\n@endif\n",
        "",
    ],
    [
        "deferred_gbuffer_solid_frag.hpsl",
        "\tuniform cTexture2D aLiquidMap : 11;\n",
        "\tuniform cTexture2D aLiquidMap : 11;\n\tuniform cVector4f px_vLiquidFade;\n",
    ],
    [
        "water_surface_frag.hpsl",
        "\t@else\n\t\tfloat fWaveT = afT;\n\t@endif",
        "\t@else\n\t\tfloat fWaveT = afT;\n\t\tfloat fFadeNear = mfFadeWhenNear;\n\t\tcVector4f vReflectionMul = avReflectionMul;\n\t@endif",
    ],
    [
        "deferred_illumination_solid_frag.hpsl",
        "\t\tvIllumination.rgb *= afIlluminationMul;\n\t@endif",
        "\t\tvIllumination.rgb *= afIlluminationMul;\n\t@else\n\t\tvIllumination.rgb *= avIlluminationMul.rgb;\n\t@endif",
    ],
    [
        "deferred_light_frag.hpsl",
        concat!(
            "\t\t\tfloat fOneMinusCos = max(0.0, 1.0 - dot( vLightDir,  avLightForward));\n",
            "\t\t\tfAttenuatuion *= pow(1.0 - sqrt(min(fOneMinusCos / afOneMinusCosHalfSpotFOV,1)), afSpotFalloffPow);"
        ),
        "\t\t\tfAttenuatuion *= pow(max(0.0, 1.0 - distance(vProjectedUv.xy, cVector2f(0.5)) * 2.0), afSpotFalloffPow);",
    ],
    [
        "deferred_light_frag.hpsl",
        "\t\tfAttenuatuion *= max(0, vProjectedUv.z);\n",
        concat!(
            "\t\tfloat fSpotZ = (1.0 - afSpotNearClip / max(fDistance * dot(vLightDir, avLightForward), 1e-4)) / (1.0 - afSpotNearClip * afInvLightRadius);\n",
            "\t\tfAttenuatuion *= max(0, fSpotZ) * clamp((1.0 - fSpotZ) * 128.0, 0.0, 1.0);\n"
        ),
    ],
    [
        "deferred_light_frag.hpsl",
        "\t@ifdef GoboType_Specular\n\t\tvDiffuse = cVector3f(fGradLen / 8.0f);\n\t@endif\n",
        "",
    ],
    [
        "deferred_light_frag.hpsl",
        concat!(
            "\t\tfLightTransport = fLightTransport * fLightTransport; // pow(fLightTransport, 8.0)\n",
            "\t\tfLightTransport = fLightTransport * fLightTransport;\n",
            "\t\tfLightTransport = fLightTransport * fLightTransport;\n",
            "\t\tfLightTransport = fLightTransport * fLightTransport;\n"
        ),
        "\t\tfLightTransport = fLightTransport < 0.0 ? 0.125 : 0.0;\n",
    ],
    [
        "deferred_light_frag.hpsl",
        "vColorVal.rgba = GammaToLinearCorrection(vColorVal.rgba);",
        "vColorVal.rgb = GammaToLinearCorrection(vColorVal.rgb);",
    ],
    [
        "deferred_light_frag.hpsl",
        "vSubColor = vColorVal.rgb - min(vColorVal.r, min(vColorVal.g, vColorVal.b)) * 0.5;",
        "vSubColor = max(vColorVal.rgb - min(vColorVal.r, min(vColorVal.g, vColorVal.b)) * 0.5, cVector3f(1e-4));",
    ],
    [
        "cache_terrain_diffuse_frag.hpsl",
        "uniform cTexture2D aAlphaMap3 : 9;",
        concat!(
            "uniform cTexture2D aAlphaMap3 : 9;\n",
            "@ifdef UseVertexPosition\n",
            "uniform cVector3f avDetailMapScale;\nuniform cVector3f avDetailFade;\nuniform cVector3f avBaseDetailAmount;\n",
            "uniform cVector3f avDetailAmount0;\nuniform cVector3f avDetailAmount1;\nuniform cVector3f avDetailAmount2;\nuniform cVector3f avDetailAmount3;\n",
            "uniform cTexture2D aDetailMap0 : 10;\nuniform cTexture2D aDetailMap1 : 11;\nuniform cTexture2D aDetailMap2 : 12;\n",
            "@endif"
        ),
    ],
    [
        "cache_terrain_diffuse_frag.hpsl",
        "in cVector4f px_vTexCoord0,",
        "in cVector4f px_vTexCoord0,\n@ifdef UseVertexPosition\n\t\t  in cVector3f px_vVertexPos,\n@endif",
    ],
    [
        "cache_terrain_diffuse_frag.hpsl",
        "vDiffuseColor = mix(vDiffuseColor, vTexColor3.xyz, vDissolveBlend.w);",
        concat!(
            "vDiffuseColor = mix(vDiffuseColor, vTexColor3.xyz, vDissolveBlend.w);\n",
            "\t@ifdef UseVertexPosition\n",
            "\t\t@ifdef UseBaseTexture\n\t\t\tcVector3f vDetailAmount = avBaseDetailAmount;\n\t\t@else\n\t\t\tcVector3f vDetailAmount = cVector3f(0);\n\t\t@endif\n",
            "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount0, vDissolveBlend.x);\n",
            "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount1, vDissolveBlend.y);\n",
            "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount2, vDissolveBlend.z);\n",
            "\t\tvDetailAmount = mix(vDetailAmount, avDetailAmount3, vDissolveBlend.w);\n",
            "\t\t@ifdef UseBaseTexture\n\t\t@else\n\t\t\tvDetailAmount /= max(min(dot(vDissolveBlend, cVector4f(1.0)), 1.0), 0.0001);\n\t\t@endif\n",
            "\t\tfloat fDetailMul = 1.0 - clamp((-px_vVertexPos.z - avDetailFade.x) / avDetailFade.y, 0.0, 1.0);\n",
            "\t\tfloat fAmountLength = length(vDetailAmount);\n",
            "\t\tcVector3f vNormalizedAmount = vDetailAmount / (fAmountLength + 0.0001);\n",
            "\t\tcVector2f vWorldXZ = (vTexCoord - cVector2f(0.5)) * avDetailFade.z;\n",
            "\t\tcVector3f vDetailColor = sqrt(pow(sample(aDetailMap0, vWorldXZ * avDetailMapScale.x).xyz, cVector3f(2.2)) * vNormalizedAmount.x +\n",
            "\t\t\t\t\t\t\t\t\t pow(sample(aDetailMap1, vWorldXZ * avDetailMapScale.y).xyz, cVector3f(2.2)) * vNormalizedAmount.y +\n",
            "\t\t\t\t\t\t\t\t\t pow(sample(aDetailMap2, vWorldXZ * avDetailMapScale.z).xyz, cVector3f(2.2)) * vNormalizedAmount.z);\n",
            "\t\tvDiffuseColor *= mix(cVector3f(1.0), vDetailColor * 2.0, fDetailMul * fAmountLength);\n",
            "\t@endif"
        ),
    ],
    [
        "cache_terrain_diffuse_frag.hpsl",
        "out_vColor.xyz =  vDiffuseColor.xyz;",
        "out_vColor.xyz = vDiffuseColor.xyz * vDiffuseColor.xyz;",
    ],
    [
        "deferred_light_frag.hpsl",
        "(vSourceUV[2] - vSourceUV[0]) * fGradient).xyz;",
        concat!(
            "(vSourceUV[2] - vSourceUV[0]) * fGradient).xyz;\n",
            "\t\t\t\tvSpecular *= clamp((0.6 - max(abs(vSourceUV[0].x - 0.5), abs(vSourceUV[0].y - 0.5))) * 10.0, 0.0, 1.0);"
        ),
    ],
    [
        "deferred_transparent_frag.hpsl",
        "\t\t@ifdef UseLinearColorSpaceCorrection\n\t\t\tvFinalColor.rgb = GammaToLinearCorrection(vFinalColor.rgb);",
        "\t\t@ifdef UseLinearColorSpaceCorrection\n\t\t@ifdef UseSRGBDiffuseMap\n\t\t@else\n\t\t\tvFinalColor.rgb = GammaToLinearCorrection(vFinalColor.rgb);\n\t\t@endif",
    ],
    [
        "deferred_transparent_frag.hpsl",
        "@ifdef UseNormalMapping || UseNormalMap\n",
        "@ifdef UseNormalMapping\n@define UseNormalMap 1\n@endif\n@ifdef UseScreenNormal\n@define RefractNormals 1\n@endif\n@ifdef UseNormalMap\n",
    ],
    [
        "deferred_transparent_frag.hpsl",
        "\t@endif\n\n\tcVector4f vFinalColor;",
        "\t@else\n\t\tfloat afLightLevel = afLightLevel;\n\t@endif\n\n\tcVector4f vFinalColor;",
    ],
];

const GAMMA_ALBEDO: [[&str; 3]; 5] = [
    [
        "deferred_gbuffer_solid_frag.hpsl",
        "vDiffuseColor *= px_vColor.xyz;",
        "vDiffuseColor *= pow(px_vColor.xyz, cVector3f(1.0 / 2.2));",
    ],
    [
        "deferred_gbuffer_decal_frag.hpsl",
        "vDiffuse * px_vColor;",
        "vDiffuse * cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);",
    ],
    [
        "deferred_projected_uv_frag.hpsl",
        "vDiffuseColor.xyz * px_vColor.xyz;",
        "vDiffuseColor.xyz * pow(px_vColor.xyz, cVector3f(1.0 / 2.2));",
    ],
    [
        "deferred_undergrowth_gbuffer_frag.hpsl",
        "vDiffuseColor *= px_vColor;",
        "vDiffuseColor *= cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);",
    ],
    [
        "deferred_terrain_gbuffer_frag.hpsl",
        "vDiffuseColor *= px_vColor;",
        "vDiffuseColor *= cVector4f(pow(px_vColor.xyz, cVector3f(1.0 / 2.2)), px_vColor.w);",
    ],
];

fn sl(d: &[u8], r: std::ops::Range<usize>) -> &[u8] {
    d.get(r).unwrap_or_default()
}

fn tail(d: &[u8], from: usize) -> &[u8] {
    d.get(from..).unwrap_or_default()
}

fn has_at(d: &[u8], at: usize, pat: &[u8]) -> bool {
    d.get(at..).is_some_and(|t| t.starts_with(pat))
}

fn find(hay: &[u8], needle: &[u8]) -> Option<usize> {
    if needle.len() > hay.len() {
        return None;
    }
    (0..=hay.len() - needle.len()).find(|&i| sl(hay, i..i + needle.len()) == needle)
}

fn rfind(hay: &[u8], needle: &[u8]) -> Option<usize> {
    if needle.len() > hay.len() {
        return None;
    }
    (0..=hay.len() - needle.len())
        .rev()
        .find(|&i| sl(hay, i..i + needle.len()) == needle)
}

fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

fn is_word(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_'
}

fn is_blank(c: u8) -> bool {
    c == b' ' || c == b'\t'
}

fn skip(d: &[u8], mut p: usize, pred: fn(u8) -> bool) -> usize {
    while let Some(&c) = d.get(p)
        && pred(c)
    {
        p += 1;
    }
    p
}

fn word_boundary(d: &[u8], p: usize) -> bool {
    let before = p
        .checked_sub(1)
        .and_then(|q| d.get(q))
        .is_some_and(|&c| is_word(c));
    let after = d.get(p).is_some_and(|&c| is_word(c));
    before != after
}

fn replace_pairs(data: &mut Bytes, table: &[[&str; 3]], file: &[u8]) {
    for [f, old, new] in table {
        if file == f.as_bytes() {
            *data = replace_string_to(data, old.as_bytes(), new.as_bytes());
        }
    }
}

pub fn fallback_name(glsl: &[u8]) -> Bytes {
    for [from, to] in ALIASES {
        if glsl == from.as_bytes() {
            return to.as_bytes().to_vec();
        }
    }
    set_file_ext(glsl, b"hpsl")
}

/// `@ifdef UseLinearColorSpaceCorrection\s*\n[^\n]*GammaToLinearCorrection[^\n]*\n\s*@elseif GammaCorrectCubeMaps`
fn collapse_cube_gamma(data: &mut Bytes) {
    const HEAD: &[u8] = b"@ifdef UseLinearColorSpaceCorrection";
    const TAIL: &[u8] = b"@elseif GammaCorrectCubeMaps";
    let mut out = Bytes::with_capacity(data.len());
    let mut i = 0;
    while let Some(&c) = data.get(i) {
        let end = has_at(data, i, HEAD)
            .then(|| cube_gamma_end(data, i + HEAD.len(), TAIL))
            .flatten();
        if let Some(end) = end {
            out.extend_from_slice(HEAD);
            i = end;
        } else {
            out.push(c);
            i += 1;
        }
    }
    *data = out;
}

fn cube_gamma_end(d: &[u8], from: usize, end_tag: &[u8]) -> Option<usize> {
    let ws_end = skip(d, from, is_space);
    let nl = from + d.get(from..ws_end)?.iter().rposition(|&c| c == b'\n')?;
    let line_end = nl + 1 + d.get(nl + 1..)?.iter().position(|&c| c == b'\n')?;
    find(d.get(nl + 1..line_end)?, b"GammaToLinearCorrection")?;
    let t = skip(d, line_end + 1, is_space);
    has_at(d, t, end_tag).then_some(t + end_tag.len())
}

struct InputDecl {
    end: usize,
    ty: (usize, usize),
    name: (usize, usize),
}

/// `\n[ \t]*in[ \t]+(\w+)[ \t]+(px_(?!vPosition)\w+(?:\[\d+\])?),[ \t]*(?=\n)` anchored at `d[at] == '\n'`.
fn match_input(d: &[u8], at: usize) -> Option<InputDecl> {
    let mut p = skip(d, at + 1, is_blank);
    if !has_at(d, p, b"in") {
        return None;
    }
    p += 2;
    let ty_start = skip(d, p, is_blank);
    if ty_start == p {
        return None;
    }
    let ty_end = skip(d, ty_start, is_word);
    if ty_end == ty_start {
        return None;
    }
    let name_start = skip(d, ty_end, is_blank);
    if name_start == ty_end
        || !has_at(d, name_start, b"px_")
        || has_at(d, name_start + 3, b"vPosition")
    {
        return None;
    }
    p = skip(d, name_start + 3, is_word);
    if p == name_start + 3 {
        return None;
    }
    if d.get(p) == Some(&b'[') {
        let digits = skip(d, p + 1, |c| c.is_ascii_digit());
        if digits == p + 1 || d.get(digits) != Some(&b']') {
            return None;
        }
        p = digits + 1;
    }
    let name_end = p;
    if d.get(p) != Some(&b',') {
        return None;
    }
    p = skip(d, p + 1, is_blank);
    (d.get(p) == Some(&b'\n')).then_some(InputDecl {
        end: p,
        ty: (ty_start, ty_end),
        name: (name_start, name_end),
    })
}

fn lift_inputs_to_uniforms(data: &mut Bytes, light: bool) {
    let mut head: Bytes = if light {
        b"@ifdef LightType_Spot || LightType_Directional\n@else\n@define LightType_Point 1\n@endif\n"
            .to_vec()
    } else {
        Bytes::new()
    };
    let mut body = Bytes::with_capacity(data.len());
    let (mut last, mut i) = (0, 0);
    while let Some(&c) = data.get(i) {
        match (c == b'\n').then(|| match_input(data, i)).flatten() {
            Some(m) => {
                let mut decl = b"uniform ".to_vec();
                decl.extend_from_slice(sl(data, m.ty.0..m.ty.1));
                decl.push(b' ');
                decl.extend_from_slice(sl(data, m.name.0..m.name.1));
                decl.extend_from_slice(b";\n");
                if find(&head, &decl).is_none() {
                    head.extend_from_slice(&decl);
                }
                body.extend_from_slice(sl(data, last..i));
                last = m.end;
                i = m.end;
            }
            None => i += 1,
        }
    }
    body.extend_from_slice(tail(data, last));
    head.extend_from_slice(&body);
    *data = head;
}

/// `\ba_mtxView(Projection)?\b` -> `a_mtxModelView$1`
fn rename_view_matrices(d: &[u8]) -> Bytes {
    const NAME: &[u8] = b"a_mtxView";
    const PROJ: &[u8] = b"Projection";
    let mut out = Bytes::with_capacity(d.len());
    let mut i = 0;
    while let Some(&c) = d.get(i) {
        if has_at(d, i, NAME) && word_boundary(d, i) {
            let p = i + NAME.len();
            let with_proj = has_at(d, p, PROJ) && word_boundary(d, p + PROJ.len());
            if with_proj || word_boundary(d, p) {
                out.extend_from_slice(b"a_mtxModelView");
                if with_proj {
                    out.extend_from_slice(PROJ);
                    i = p + PROJ.len();
                } else {
                    i = p;
                }
                continue;
            }
        }
        out.push(c);
        i += 1;
    }
    out
}

/// `\bavBlendVec<axis>\b` -> `mul(cMatrix3f(a_mtxNormalFrag), cVector3f(<vec>))`
fn replace_blend_vec(d: &[u8], axis: u8, vec: &str) -> Bytes {
    let name = [b"avBlendVec".as_slice(), &[axis]].concat();
    let to = format!("mul(cMatrix3f(a_mtxNormalFrag), cVector3f({vec}))");
    let mut out = Bytes::with_capacity(d.len());
    let mut i = 0;
    while let Some(&c) = d.get(i) {
        if has_at(d, i, &name) && word_boundary(d, i) && word_boundary(d, i + name.len()) {
            out.extend_from_slice(to.as_bytes());
            i += name.len();
        } else {
            out.push(c);
            i += 1;
        }
    }
    out
}

fn patch_projected_uv_vertex(data: &mut Bytes) {
    const END: &[u8] = b"int lBoneOffset = lBaseOffset;\n\t@endif\n";
    let begin = find(data, b"\t@ifdef UseInstanceOffset\n\t\tint lBaseOffset");
    let end = find(data, END);
    let (Some(begin), Some(end)) = (begin, end) else {
        return;
    };
    // an end before the begin wraps like the C++ size_t, erasing to the end
    let len = (end + END.len())
        .wrapping_sub(begin)
        .min(data.len() - begin);
    data.drain(begin..begin + len);
    let world = replace_string_to(
        data,
        b"mul(mtxModel, cVector4f(vtx_vPosition.xyz, 1.0))",
        b"cVector4f(vtx_vPosition.xyz, 1.0)",
    );
    let mut out = b"uniform cVector4f avColorMul;\nuniform cMatrixf a_mtxModelViewProjection;\nuniform cMatrixf a_mtxModelView;\nuniform cMatrixf a_mtxNormal;\nuniform cMatrixf a_mtxModel;\n".to_vec();
    out.extend(rename_view_matrices(&world));
    *data = replace_string_to(
        &out,
        b"px_vTexCoord0 = vLocalVertexPos;",
        b"px_vTexCoord0 = mul(a_mtxModel, vLocalVertexPos);",
    );
}

/// `debug` is `OPENHPL_HPSL_PATCH` ("file|old|new") or empty.
pub fn patch_hpsl(file: &[u8], data: &mut Bytes, hw_srgb: bool, debug: &[u8]) {
    data.retain(|&c| c != b'\r');
    replace_pairs(data, &REPLACEMENTS, file);
    if !hw_srgb {
        replace_pairs(data, &GAMMA_ALBEDO, file);
    }
    collapse_cube_gamma(data);
    for [f, anchor, insert] in PATCHES {
        if file == f.as_bytes()
            && let Some(p) = rfind(data, anchor.as_bytes())
        {
            data.splice(p..p, insert.bytes());
        }
    }
    let light = file == b"deferred_light_frag.hpsl";
    if (light || file == b"deferred_light_box_frag.hpsl")
        && find(data, b"UseTextureBuffer").is_none()
    {
        lift_inputs_to_uniforms(data, light);
    }
    if file == b"deferred_projected_uv_vtx.hpsl" {
        patch_projected_uv_vertex(data);
    }
    if file == b"deferred_projected_uv_frag.hpsl" && find(data, b"avBlendVecX").is_some() {
        for (axis, vec) in [(b'X', "1, 0, 0"), (b'Y', "0, 1, 0"), (b'Z', "0, 0, 1")] {
            *data = replace_blend_vec(data, axis, vec);
        }
        data.splice(0..0, b"uniform cMatrixf a_mtxNormalFrag;\n".iter().copied());
    }
    if file == b"posteffect_image_trail_frag.hpsl" {
        *data = replace_string_to(
            data,
            b"mix(vPrevDiffuseColor.rgb, vDiffuseColor.rgb, afAlpha), 1.0)",
            b"vDiffuseColor.rgb, afAlpha)",
        );
    }
    apply_debug_patch(file, data, debug);
}

fn apply_debug_patch(file: &[u8], data: &mut Bytes, debug: &[u8]) {
    let Some(a) = debug.iter().position(|&c| c == b'|') else {
        return;
    };
    let Some(b) = tail(debug, a + 1)
        .iter()
        .position(|&c| c == b'|')
        .map(|i| i + a + 1)
    else {
        return;
    };
    if file != sl(debug, 0..a) {
        return;
    }
    let old = sl(debug, a + 1..b);
    if let Some(p) = rfind(data, old) {
        data.splice(p..p + old.len(), tail(debug, b + 1).iter().copied());
    }
}

/// Rebirth reads BC5 as RG; the first `avNormalValue.yxx` becomes the swizzle for SOMA's LATC2 layout.
pub fn fix_bc5(parsed: &mut Bytes) {
    if let Some(p) = find(parsed, b"avNormalValue.yxx") {
        parsed.splice(p..p + 17, b"avNormalValue.wyx".iter().copied());
    }
}

/// `uniform\s+cTexture\w*\s+(\w+)\s*:\s*(\d+)\s*;` -> (sampler, unit)
pub fn texture_bindings(d: &[u8]) -> Vec<(&[u8], i32)> {
    const KW: &[u8] = b"uniform";
    let mut out = Vec::new();
    let mut i = 0;
    while i < d.len() {
        if has_at(d, i, KW)
            && let Some((end, name, unit)) = match_binding(d, i + KW.len())
        {
            out.push((name, unit));
            i = end;
        } else {
            i += 1;
        }
    }
    out
}

fn match_binding(d: &[u8], at: usize) -> Option<(usize, &[u8], i32)> {
    let p = skip(d, at, is_space);
    if p == at || !has_at(d, p, b"cTexture") {
        return None;
    }
    let p = skip(d, p + 8, is_word);
    let name_start = skip(d, p, is_space);
    if name_start == p {
        return None;
    }
    let name_end = skip(d, name_start, is_word);
    if name_end == name_start {
        return None;
    }
    let colon = skip(d, name_end, is_space);
    if d.get(colon) != Some(&b':') {
        return None;
    }
    let digits = skip(d, colon + 1, is_space);
    let digits_end = skip(d, digits, |c| c.is_ascii_digit());
    if digits_end == digits {
        return None;
    }
    let semi = skip(d, digits_end, is_space);
    (d.get(semi) == Some(&b';')).then(|| {
        (
            semi + 1,
            sl(d, name_start..name_end),
            token_to_int(sl(d, digits..digits_end)),
        )
    })
}

/// `sampler_<name>` entries of the preprocessor's define map (key order) -> (name, unit).
pub fn sampler_units<'a>(vars: impl Iterator<Item = (&'a [u8], &'a [u8])>) -> Vec<(&'a [u8], i32)> {
    vars.filter(|(k, _)| !k.is_empty())
        .filter_map(|(k, v)| {
            let mut parts = split(k, Some(b"_".as_slice()), true);
            if parts.next()? != b"sampler".as_slice() {
                return None;
            }
            Some((parts.next()?, token_to_int(v)))
        })
        .collect()
}

mod ffi {
    use super::{Bytes, fallback_name, fix_bc5, patch_hpsl, sampler_units, texture_bindings};
    use crate::string_ffi::{Sink, emit, units};
    use std::ffi::c_void;

    type Pair = unsafe extern "C" fn(*mut c_void, *const u8, usize, i32);

    #[repr(C)]
    pub struct Kv {
        k: *const u8,
        kn: usize,
        v: *const u8,
        vn: usize,
    }

    /// # Safety
    /// Pointers valid for their lengths; `sink` accepts `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_shader_fallback_name(
        name: *const u8,
        n: usize,
        sink: Sink<u8>,
        ctx: *mut c_void,
    ) {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, &fallback_name(units(name, n))) }
    }

    /// Patches the HPSL source `data` of `file`; `debug` is `OPENHPL_HPSL_PATCH` (empty if unset).
    ///
    /// # Safety
    /// Pointers valid for their lengths; `sink` accepts `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_shader_patch_hpsl(
        file: *const u8,
        file_n: usize,
        data: *const u8,
        data_n: usize,
        hw_srgb: bool,
        debug: *const u8,
        debug_n: usize,
        sink: Sink<u8>,
        ctx: *mut c_void,
    ) {
        // SAFETY: caller contract.
        unsafe {
            let mut d: Bytes = units(data, data_n).to_vec();
            patch_hpsl(units(file, file_n), &mut d, hw_srgb, units(debug, debug_n));
            emit(sink, ctx, &d);
        }
    }

    /// # Safety
    /// Pointers valid for their lengths; `sink` accepts `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_shader_fix_bc5(
        data: *const u8,
        n: usize,
        sink: Sink<u8>,
        ctx: *mut c_void,
    ) {
        // SAFETY: caller contract.
        unsafe {
            let mut d: Bytes = units(data, n).to_vec();
            fix_bc5(&mut d);
            emit(sink, ctx, &d);
        }
    }

    /// Calls `cb(ctx, sampler, sampler_len, unit)` per `uniform cTexture* name : unit;` in `text`.
    ///
    /// # Safety
    /// Pointers valid for their lengths; `cb` accepts `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_shader_texture_bindings(
        text: *const u8,
        n: usize,
        cb: Pair,
        ctx: *mut c_void,
    ) {
        // SAFETY: caller contract.
        unsafe {
            for (name, unit) in texture_bindings(units(text, n)) {
                cb(ctx, name.as_ptr(), name.len(), unit);
            }
        }
    }

    /// Calls `cb` per `sampler_<name>` entry of the key-ordered define map `vars`.
    ///
    /// # Safety
    /// `vars` holds `n` entries valid for their lengths; `cb` accepts `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_shader_sampler_units(
        vars: *const Kv,
        n: usize,
        cb: Pair,
        ctx: *mut c_void,
    ) {
        // SAFETY: caller contract.
        unsafe {
            let kv = units(vars, n);
            let it = kv.iter().map(|e| (units(e.k, e.kn), units(e.v, e.vn)));
            for (name, unit) in sampler_units(it) {
                cb(ctx, name.as_ptr(), name.len(), unit);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn patched(file: &str, src: &str, srgb: bool, debug: &str) -> String {
        let mut d = src.as_bytes().to_vec();
        patch_hpsl(file.as_bytes(), &mut d, srgb, debug.as_bytes());
        String::from_utf8(d).unwrap()
    }

    #[test]
    fn fallback_names() {
        let n = |s: &str| String::from_utf8(fallback_name(s.as_bytes())).unwrap();
        assert_eq!(n("gui_vtx.glsl"), "base_vtx.hpsl");
        assert_eq!(
            n("deferred_decal_frag.glsl"),
            "deferred_gbuffer_decal_frag.hpsl"
        );
        assert_eq!(n("foo_frag.glsl"), "foo_frag.hpsl");
        assert_eq!(n("noext"), "noext.hpsl");
        assert_eq!(n("a.b.glsl"), "a.b.hpsl");
    }

    #[test]
    fn texture_binding_scan() {
        let src = b"uniform cTexture2D aDiffuse : 3;\nuniform\tcTextureCube\n aEnv:12 ;\nuniform cTexture2D x : ;\nuniform float cTexture2D y : 1;\nxuniform cTexture3D z:7;";
        let got: Vec<(&[u8], i32)> = texture_bindings(src);
        assert_eq!(got, [(&b"aDiffuse"[..], 3), (b"aEnv", 12), (b"z", 7)]);
    }

    #[test]
    fn sampler_unit_scan() {
        let vars: [(&[u8], &[u8]); 6] = [
            (b"", b"1"),
            (b"sampler_a", b"4"),
            (b"sampler_b_c", b"x"),
            (b"sampler", b"9"),
            (b"Sampler_d", b"9"),
            (b"__sampler__e", b"2"),
        ];
        let got = sampler_units(vars.iter().copied());
        assert_eq!(got, [(&b"a"[..], 4), (b"b", 0), (b"e", 2)]);
    }

    #[test]
    fn bc5_swizzle_first_only() {
        let mut d = b"avNormalValue.yxx avNormalValue.yxx".to_vec();
        fix_bc5(&mut d);
        assert_eq!(d, b"avNormalValue.wyx avNormalValue.yxx");
    }

    #[test]
    fn cube_gamma_collapses_only_full_pattern() {
        let a = "@ifdef UseLinearColorSpaceCorrection \n\n  x = GammaToLinearCorrection(x);\n \t@elseif GammaCorrectCubeMaps\nrest";
        assert_eq!(
            patched("any.hpsl", a, true, ""),
            "@ifdef UseLinearColorSpaceCorrection\nrest"
        );
        let b = "@ifdef UseLinearColorSpaceCorrection\nx = y;\n@elseif GammaCorrectCubeMaps\n";
        assert_eq!(patched("any.hpsl", b, true, ""), b);
        let c = "@ifdef UseLinearColorSpaceCorrection\nGammaToLinearCorrection\n  @elseif GammaCorrectCubeMaps";
        assert_eq!(
            patched("any.hpsl", c, true, ""),
            "@ifdef UseLinearColorSpaceCorrection"
        );
    }

    #[test]
    fn carriage_returns_removed_everywhere() {
        assert_eq!(patched("other.hpsl", "a\r\nb\r", true, ""), "a\nb");
    }

    #[test]
    fn light_inputs_become_uniforms() {
        let src = "x\n  in cVector4f px_vA,  \n\tin float px_vB[3],\nin float px_vPosition,\nin float px_vPositionX,\n in int px_vA,\nin float px_vC[x],\nin float px_vD,\ny";
        let want = "@ifdef LightType_Spot || LightType_Directional\n@else\n@define LightType_Point 1\n@endif\nuniform cVector4f px_vA;\nuniform float px_vB[3];\nuniform int px_vA;\nuniform float px_vD;\nx\nin float px_vPosition,\nin float px_vPositionX,\nin float px_vC[x],\ny";
        assert_eq!(patched("deferred_light_frag.hpsl", src, true, ""), want);
        let box_want = "uniform cVector4f px_vA;\nuniform float px_vB[3];\nuniform int px_vA;\nuniform float px_vD;\nx\nin float px_vPosition,\nin float px_vPositionX,\nin float px_vC[x],\ny";
        assert_eq!(
            patched("deferred_light_box_frag.hpsl", src, true, ""),
            box_want
        );
        let tb = "UseTextureBuffer\nin float px_vD,\n";
        assert_eq!(patched("deferred_light_box_frag.hpsl", tb, true, ""), tb);
    }

    #[test]
    fn projected_uv_vertex_rewrites_matrices() {
        let src = "A\n\t@ifdef UseInstanceOffset\n\t\tint lBaseOffset = 0;\n\t\tint lBoneOffset = lBaseOffset;\n\t@endif\nmul(mtxModel, cVector4f(vtx_vPosition.xyz, 1.0)) a_mtxView a_mtxViewProjection a_mtxViewX xa_mtxView a_mtxViewProjectionX px_vTexCoord0 = vLocalVertexPos;";
        let got = patched("deferred_projected_uv_vtx.hpsl", src, true, "");
        assert!(got.starts_with("uniform cVector4f avColorMul;\n"));
        assert!(got.ends_with("A\ncVector4f(vtx_vPosition.xyz, 1.0) a_mtxModelView a_mtxModelViewProjection a_mtxViewX xa_mtxView a_mtxViewProjectionX px_vTexCoord0 = mul(a_mtxModel, vLocalVertexPos);"));
    }

    #[test]
    fn projected_uv_vertex_end_before_begin_erases_tail() {
        let src = "int lBoneOffset = lBaseOffset;\n\t@endif\nZ\t@ifdef UseInstanceOffset\n\t\tint lBaseOffset tail";
        let got = patched("deferred_projected_uv_vtx.hpsl", src, true, "");
        assert!(got.ends_with(
            "uniform cMatrixf a_mtxModel;\nint lBoneOffset = lBaseOffset;\n\t@endif\nZ"
        ));
    }

    #[test]
    fn projected_uv_fragment_blend_vectors() {
        let src = "avBlendVecX avBlendVecY2 xavBlendVecZ avBlendVecZ.";
        assert_eq!(
            patched("deferred_projected_uv_frag.hpsl", src, true, ""),
            "uniform cMatrixf a_mtxNormalFrag;\nmul(cMatrix3f(a_mtxNormalFrag), cVector3f(1, 0, 0)) avBlendVecY2 xavBlendVecZ mul(cMatrix3f(a_mtxNormalFrag), cVector3f(0, 0, 1))."
        );
    }

    #[test]
    fn gamma_albedo_only_without_hardware_srgb() {
        let src = "vDiffuseColor *= px_vColor.xyz;";
        let want = "vDiffuseColor *= pow(px_vColor.xyz, cVector3f(1.0 / 2.2));";
        assert_eq!(
            patched("deferred_gbuffer_solid_frag.hpsl", src, false, ""),
            want
        );
        assert_eq!(
            patched("deferred_gbuffer_solid_frag.hpsl", src, true, ""),
            src
        );
    }

    #[test]
    fn anchored_insert_uses_last_occurrence() {
        let src = "\tvDiffuse *= fAttenuatuion;\n\tvDiffuse *= fAttenuatuion;";
        let got = patched("deferred_light_frag.hpsl", src, true, "");
        assert_eq!(got.matches("\t@ifdef BoxMask").count(), 1);
        assert!(got.contains("@endif\n\tvDiffuse *= fAttenuatuion;\n\t@ifdef BoxMask"));
    }

    #[test]
    fn debug_patch_replaces_last_match_for_named_file() {
        let src = "aXa X";
        assert_eq!(patched("f.hpsl", src, true, "f.hpsl|X|yy"), "aXa yy");
        assert_eq!(patched("f.hpsl", src, true, "g.hpsl|X|yy"), src);
        assert_eq!(patched("f.hpsl", src, true, "f.hpsl|X"), src);
        assert_eq!(patched("f.hpsl", src, true, "f.hpsl||!"), "aXa X!");
        assert_eq!(patched("f.hpsl", src, true, "noseparator"), src);
    }
}
