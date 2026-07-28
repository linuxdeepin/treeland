// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    vec2 lightDirection;
    float radius;
    float bezelWidth;
    float thickness;
    float ior;
    float specular;
    float tint;
    float contentEdgePull;    // fraction of optical pull kept at the glass lip (0–1)
    float contentRampEnd;     // t in bezel where content pull reaches full (0–1)
    float refractionMaxTan;   // cap on geometric slope tan(theta)
    float profilePower;       // surface profile exponent (default 4.0 = squircle)
    float innerShadow;        // inner shadow intensity (0–1, default 0.25)
} ubuf;

layout(binding = 1) uniform sampler2D source;

const float kSqrt2 = 1.4142135623730951;
const float kInvSqrt2 = 0.7071067811865476;

vec2 halfSize = max(ubuf.itemSize, vec2(1.0)) * 0.5;

float sdRoundedRect(vec2 p, vec2 halfSize, float r)
{
    float rr = min(max(r, 0.0), min(halfSize.x, halfSize.y));
    vec2 q = abs(p) - halfSize + rr;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rr;
}

// Analytic gradient of sdRoundedRect w.r.t. p (outward-pointing, unit length on the surface).
vec2 sdRoundedRectGrad(vec2 p, vec2 halfSize, float r)
{
    float rr = min(max(r, 0.0), min(halfSize.x, halfSize.y));
    vec2 ap = abs(p);
    vec2 q = ap - halfSize + rr;
    vec2 s = vec2(p.x < 0.0 ? -1.0 : 1.0, p.y < 0.0 ? -1.0 : 1.0);

    if (min(q.x, q.y) > 0.0) {
        float ql = length(q);
        return s * (q / max(ql, 1e-6));
    }

    // Flat / interior: sd = max(q.x, q.y) - rr when inside the expanded rect.
    if (q.x > q.y)
        return vec2(s.x, 0.0);
    if (q.y > q.x)
        return vec2(0.0, s.y);
    // Tie on the diagonal — any convex combination is a subgradient; pick the bisector.
    return s * kInvSqrt2;
}

// ── Linear bezel field ────────────────────────────────────────────────
float fieldBezel()
{
    float limit = min(halfSize.x, halfSize.y);
    return clamp(ubuf.bezelWidth, 1.0, max(limit - 1.0, 1.0));
}

float fieldT(vec2 p, float rr)
{
    float limit = min(halfSize.x, halfSize.y);
    float r = clamp(rr, 0.0, limit);
    float b = fieldBezel();
    return clamp(-sdRoundedRect(p, halfSize, r) / b, 0.0, 1.0);
}

// Outward unit normal of the bezel field (matches previous -normalize(∇fieldT)).
vec2 fieldNormal(vec2 p, float rr)
{
    vec2 g = sdRoundedRectGrad(p, halfSize, rr);
    float len = length(g);
    return len > 1e-5 ? (g / len) : vec2(0.0);
}

// ── Rounded-square Coons map (actual → virtual) ───────────────────────
struct BoundaryResult {
    vec2 point;
    vec2 derivative;
};

struct RoundedMapResult {
    vec2 point;
    vec4 jacobian;
};

float cornerQuintic(float value)
{
    float s = clamp(value, 0.0, 1.0);
    return s * s * s * (s * (s * 6.0 - 15.0) + 10.0);
}

float cornerQuinticDerivative(float value)
{
    float s = clamp(value, 0.0, 1.0);
    return 30.0 * s * s * (s - 1.0) * (s - 1.0);
}

float cornerArcParameter(float value)
{
    float s = clamp(value, 0.0, 1.0);
    float tangent = kSqrt2;
    float s2 = s * s;
    float s3 = s2 * s;
    return (s3 - 2.0 * s2 + s) * tangent
        + (-2.0 * s3 + 3.0 * s2)
        + (s3 - s2) * tangent;
}

float cornerArcParameterDerivative(float value)
{
    float s = clamp(value, 0.0, 1.0);
    float tangent = kSqrt2;
    return (3.0 * s * s - 4.0 * s + 1.0) * tangent
        + (-6.0 * s * s + 6.0 * s)
        + (3.0 * s * s - 2.0 * s) * tangent;
}

BoundaryResult cornerUnitArc(vec2 start, vec2 end, float parameter, float parameterDerivative)
{
    vec2 delta = end - start;
    vec2 raw = start + delta * parameter;
    float rawLength = length(raw);
    vec2 direction = raw / rawLength;
    vec2 rawDerivative = delta * parameterDerivative;
    float parallel = dot(direction, rawDerivative);
    BoundaryResult result;
    result.point = direction;
    result.derivative = (rawDerivative - direction * parallel) / rawLength;
    return result;
}

BoundaryResult cornerTopBoundary(float value, float alpha)
{
    BoundaryResult result;
    if (alpha <= 1e-5 || value <= 1.0 - alpha) {
        result.point = vec2(value, 1.0);
        result.derivative = vec2(1.0, 0.0);
        return result;
    }
    float local = (value - (1.0 - alpha)) / alpha;
    BoundaryResult arc = cornerUnitArc(vec2(0.0, 1.0), vec2(kInvSqrt2),
                                       cornerArcParameter(local), cornerArcParameterDerivative(local));
    result.point = vec2(1.0 - alpha) + alpha * arc.point;
    result.derivative = arc.derivative;
    return result;
}

BoundaryResult cornerRightBoundary(float value, float alpha)
{
    BoundaryResult result;
    if (alpha <= 1e-5 || value <= 1.0 - alpha) {
        result.point = vec2(1.0, value);
        result.derivative = vec2(0.0, 1.0);
        return result;
    }
    float local = (value - (1.0 - alpha)) / alpha;
    BoundaryResult arc = cornerUnitArc(vec2(1.0, 0.0), vec2(kInvSqrt2),
                                       cornerArcParameter(local), cornerArcParameterDerivative(local));
    result.point = vec2(1.0 - alpha) + alpha * arc.point;
    result.derivative = arc.derivative;
    return result;
}

RoundedMapResult roundedSquareMap(vec2 canonical, float alpha)
{
    float u = canonical.x;
    float v = canonical.y;
    BoundaryResult top = cornerTopBoundary(u, alpha);
    BoundaryResult right = cornerRightBoundary(v, alpha);
    vec2 topDelta = top.point - vec2(u, 1.0);
    vec2 rightDelta = right.point - vec2(1.0, v);
    float cornerDelta = alpha * (kInvSqrt2 - 1.0);
    float hu = cornerQuintic(u);
    float hv = cornerQuintic(v);
    float dhu = cornerQuinticDerivative(u);
    float dhv = cornerQuinticDerivative(v);
    vec2 topDerivative = top.derivative - vec2(1.0, 0.0);
    vec2 rightDerivative = right.derivative - vec2(0.0, 1.0);
    RoundedMapResult result;
    result.point = canonical
        + hv * topDelta
        + hu * rightDelta
        - hu * hv * vec2(cornerDelta);
    result.jacobian = vec4(
        1.0 + hv * topDerivative.x + dhu * rightDelta.x - dhu * hv * cornerDelta,
        dhv * topDelta.x + hu * rightDerivative.x - hu * dhv * cornerDelta,
        hv * topDerivative.y + dhu * rightDelta.y - dhu * hv * cornerDelta,
        1.0 + dhv * topDelta.y + hu * rightDerivative.y - hu * dhv * cornerDelta
    );
    return result;
}

vec4 inverseCornerJacobian(vec4 jacobian)
{
    float determinant = jacobian.x * jacobian.w - jacobian.y * jacobian.z;
    return vec4(jacobian.w, -jacobian.y, -jacobian.z, jacobian.x) / determinant;
}

vec2 cornerDiscToSquare(vec2 point)
{
    float root2 = kSqrt2;
    float a = 2.0 + point.x * point.x - point.y * point.y;
    float b = 2.0 - point.x * point.x + point.y * point.y;
    return 0.5 * vec2(
        sqrt(max(a + 2.0 * root2 * point.x, 0.0))
            - sqrt(max(a - 2.0 * root2 * point.x, 0.0)),
        sqrt(max(b + 2.0 * root2 * point.y, 0.0))
            - sqrt(max(b - 2.0 * root2 * point.y, 0.0))
    );
}

vec2 invertRoundedSquare(vec2 point, float alpha)
{
    float corner = 1.0 - alpha + alpha * kInvSqrt2;
    if (distance(point, vec2(corner)) <= 1e-5)
        return vec2(1.0);
    vec2 discInitial = cornerDiscToSquare(point);
    vec2 canonical = clamp(mix(point, discInitial, alpha), vec2(0.0), vec2(1.0));
    // Good disc initial guess typically converges in 3–4 steps; keep 5 with error exit.
    for (int iteration = 0; iteration < 5; ++iteration) {
        RoundedMapResult mapped = roundedSquareMap(canonical, alpha);
        vec2 error = mapped.point - point;
        if (dot(error, error) <= 1e-10)
            break;
        vec4 inverse = inverseCornerJacobian(mapped.jacobian);
        canonical = clamp(
            canonical - vec2(inverse.x * error.x + inverse.y * error.y,
                             inverse.z * error.x + inverse.w * error.y),
            vec2(0.0), vec2(1.0)
        );
    }
    return canonical;
}

vec2 mapActualToVirtualPoint(vec2 p, float fromR, float toR, float bezel)
{
    float limit = min(halfSize.x, halfSize.y);
    float from = clamp(fromR, 0.0, limit);
    float to = clamp(toR, 0.0, limit);
    if (abs(from - to) <= 1e-4)
        return p;

    float extent = min(max(max(max(bezel, from), to), 1.0), limit);
    vec2 pointSign = vec2(p.x < 0.0 ? -1.0 : 1.0, p.y < 0.0 ? -1.0 : 1.0);
    vec2 distanceFromEdge = halfSize - abs(p);
    if (distanceFromEdge.x >= extent || distanceFromEdge.y >= extent)
        return p;

    vec2 normalized = vec2(1.0) - distanceFromEdge / extent;
    vec2 canonical = invertRoundedSquare(normalized, from / extent);
    vec2 target = roundedSquareMap(canonical, to / extent).point;
    return pointSign * (halfSize - vec2(extent) + target * extent);
}

// ── Corner-depth preservation ─────────────────────────────────────────
float postprocessCornerDepth(float xProgress, float yProgress)
{
    float x = clamp(xProgress, 0.0, 1.0);
    float y = clamp(yProgress, 0.0, 1.0);
    float lower = min(x, y);
    float difference = abs(x - y);
    float blendWidth = 6.0 * x * y * (1.0 - x) * (1.0 - y);
    if (blendWidth <= 1e-5 || difference >= blendWidth)
        return lower;
    float blendProgress = difference / blendWidth;
    float remaining = 1.0 - blendProgress;
    return clamp(lower + 0.5 * difference * remaining * remaining, 0.0, 1.0);
}

float arcOuter(float d, float r)
{
    return d < r ? r - sqrt(max(0.0, d * (2.0 * r - d))) : 0.0;
}

float sourcePostprocessDepth(vec2 p, float rr, float bezel)
{
    if (rr >= bezel)
        return fieldT(p, rr);
    vec2 d = halfSize - abs(p);
    float topProgress = clamp((d.y - arcOuter(d.x, rr)) / bezel, 0.0, 1.0);
    float sideProgress = clamp((d.x - arcOuter(d.y, rr)) / bezel, 0.0, 1.0);
    return postprocessCornerDepth(topProgress, sideProgress);
}

vec2 preservePostprocessDepth(vec2 actualPoint, vec2 mappedPoint, float actualRadius, float opticalRadius, float bezel)
{
    float strength = clamp((opticalRadius - actualRadius) / max(bezel, 1.0), 0.0, 1.0);
    if (strength <= 1e-5)
        return mappedPoint;

    float extent = min(max(max(max(bezel, actualRadius), opticalRadius), 1.0), min(halfSize.x, halfSize.y));
    vec2 edgeDistance = halfSize - abs(actualPoint);
    if (edgeDistance.x >= extent || edgeDistance.y >= extent)
        return mappedPoint;

    float baseT = fieldT(mappedPoint, opticalRadius);
    float sourceT = sourcePostprocessDepth(actualPoint, actualRadius, bezel);
    float targetT = mix(baseT, sourceT, strength);
    vec2 result = mappedPoint;
    float b = fieldBezel();
    for (int iteration = 0; iteration < 3; ++iteration) {
        float sdVal = sdRoundedRect(result, halfSize, opticalRadius);
        float currentT = clamp(-sdVal / b, 0.0, 1.0);
        float error = currentT - targetT;
        if (abs(error) <= 1e-5)
            break;

        // ∇fieldT ≈ -∇sd / bezel inside the unclamped band; sd is 1-Lipschitz.
        vec2 gsd = sdRoundedRectGrad(result, halfSize, opticalRadius);
        vec2 g = -gsd / b;
        float gradientSquared = dot(g, g);
        if (gradientSquared <= 1e-10)
            break;
        float stepLength = clamp(error / gradientSquared, -bezel * bezel, bezel * bezel);
        result -= g * stepLength;
    }
    return result;
}

// ── Surface profile ───────────────────────────────────────────────────
vec2 surfaceProfile(float t)
{
    float pp = max(ubuf.profilePower, 1.0);
    float s = 1.0 - clamp(t, 0.0, 1.0);
    float sp = pow(s, pp);
    float inside = max(1.0 - sp, 1e-4);
    float height = pow(inside, 1.0 / pp);
    float derivative = pow(s, pp - 1.0) * height / inside;
    return vec2(height, derivative);
}

vec3 sampleBg(vec2 uv)
{
    return texture(source, clamp(uv, vec2(0.002), vec2(0.998))).rgb;
}

vec4 finishColor(vec3 color, float shapeAlpha)
{
    if (ubuf.tint > 1e-4)
        color = mix(color, vec3(1.0), clamp(ubuf.tint, 0.0, 1.0));
    float alpha = shapeAlpha * ubuf.qt_Opacity;
    return vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
}

// ── Main ──────────────────────────────────────────────────────────────
void main()
{
    vec2 size = max(ubuf.itemSize, vec2(1.0));
    vec2 p = texCoord * size - size * 0.5;

    float outerRadius = min(max(ubuf.radius, 0.0), min(halfSize.x, halfSize.y));
    float sd = sdRoundedRect(p, halfSize, outerRadius);

    // Anti-aliased shape edge.
    float scale = length(vec2(dFdx(sd), dFdy(sd)));
    float edgeAA = max(scale * 2.0, 2.0);
    float shapeAlpha = smoothstep(edgeAA, -edgeAA, sd);

    if (shapeAlpha <= 0.0) {
        fragColor = vec4(0.0);
        return;
    }

    float configuredBezel = max(ubuf.bezelWidth, 1.0);
    float maxSizeBezel = max(min(halfSize.x, halfSize.y) - 1.0, 1.0);
    float bezel = min(configuredBezel, maxSizeBezel);
    // virtualRadius is private: radius < bezel → follow bezel, else follow radius.
    float opticalRadius = clamp(max(outerRadius, bezel), 0.0, min(halfSize.x, halfSize.y));

    float effectExtent = bezel;
    if (ubuf.specular > 1e-4)
        effectExtent = max(effectExtent, 5.0);

    // Parameterized ring margin so the cheap center path never skips pixels that
    // mapping/preserve could still pull into the bezel effect band.
    //
    // radiusLift = opticalRadius - outerRadius (0 when radius >= bezel).
    // Bounds used (all scale with lift; zero when mapping is identity):
    //   • fixed-p SDF shrink when r rises:     (√2 - 1) * lift
    //   • Coons corner boundary displacement:  (1 - 1/√2) * lift
    //   • preserve depth correction travel:    lift
    // Sum = lift * (1 + 1/√2).
    float radiusLift = max(opticalRadius - outerRadius, 0.0);
    float safetyMargin = radiusLift * (1.0 + kInvSqrt2);

    float actualDistFromEdge = -sd;
    if (actualDistFromEdge > effectExtent + safetyMargin) {
        fragColor = finishColor(sampleBg(texCoord), shapeAlpha);
        return;
    }

    // Deform actual coordinates to virtual optical coordinates.
    vec2 mappedPoint = mapActualToVirtualPoint(p, outerRadius, opticalRadius, bezel);
    vec2 shadePoint = preservePostprocessDepth(p, mappedPoint, outerRadius, opticalRadius, bezel);
    float opticalDistFromEdge = -sdRoundedRect(shadePoint, halfSize, opticalRadius);

    if (opticalDistFromEdge > effectExtent) {
        fragColor = finishColor(sampleBg(texCoord), shapeAlpha);
        return;
    }

    float t = fieldT(shadePoint, opticalRadius);
    vec2 n2 = fieldNormal(shadePoint, opticalRadius);

    float maxTan = max(ubuf.refractionMaxTan, 0.1);

    vec2 profile = surfaceProfile(t);
    float h = profile.x;

    float thick = max(ubuf.thickness, 0.0);
    float edgePull = clamp(ubuf.contentEdgePull, 0.0, 1.0);
    float outerSoft = smoothstep(0.0, max(2.5 * edgeAA, 2.5), opticalDistFromEdge);
    float slopeSoft = mix(outerSoft, 1.0, edgePull);
    float rawTan = profile.y * (thick / bezel);
    float slopeMag = min(rawTan, maxTan) * slopeSoft;

    float H = h * thick;

    float rampEnd = clamp(ubuf.contentRampEnd, 0.001, 1.0);
    // edgePull=0 → contentRamp = smoothstep(0, rampEnd, t); keep exact mix for pull>0.
    float contentRamp = (edgePull <= 1e-5)
        ? smoothstep(0.0, rampEnd, t)
        : mix(edgePull, 1.0, smoothstep(0.0, rampEnd, t));
    float maxDisp = min(min(bezel * 0.85, thick * 0.75), 48.0)
        * max(maxTan / 2.75, 1.0);

    float iorG = max(ubuf.ior, 1.0001);

    float incidentInvLen = inversesqrt(slopeMag * slopeMag + 1.0);
    float sinI = slopeMag * incidentInvLen;
    float sinT = clamp(sinI / iorG, 0.0, 0.999);
    float tanT = sinT * inversesqrt(max(1.0 - sinT * sinT, 1e-4));
    float magG = H * contentRamp * max(slopeMag - tanT, 0.0);
    magG = min(magG, maxDisp);
    vec2 inward = -n2;
    vec2 offG = inward * magG;

    vec2 baseUv = shadePoint / size + 0.5;
    // sampleBg clamps; avoid a second clamp here.
    vec3 color = sampleBg(baseUv + offG / size);

    if (ubuf.specular > 1e-4) {
        vec3 N = normalize(vec3(n2 * slopeMag, 1.0));
        float oneMinusCos = 1.0 - clamp(N.z, 0.0, 1.0);
        float oneMinusCos2 = oneMinusCos * oneMinusCos;
        float fresnel = 0.04 + 0.96 * oneMinusCos2 * oneMinusCos2 * oneMinusCos;
        vec2 lightDir = normalize(ubuf.lightDirection);
        float rimDot = abs(dot(n2, lightDir));
        float rimFalloff = 1.0 - smoothstep(0.0, bezel * 0.45, opticalDistFromEdge);
        float specHighlight = fresnel * pow(rimDot * rimFalloff, 1.35);
        color += vec3(specHighlight * clamp(ubuf.specular, 0.0, 1.0));

        float innerRim = smoothstep(0.0, 2.0, opticalDistFromEdge)
            * (1.0 - smoothstep(2.0, 5.0, opticalDistFromEdge));
        color += vec3(innerRim * 0.12 * clamp(ubuf.specular, 0.0, 1.0));
    }

    if (ubuf.innerShadow > 1e-4) {
        float innerShadowMask = 1.0 - smoothstep(0.0, bezel * 0.6, opticalDistFromEdge);
        color *= mix(1.0, 0.75, innerShadowMask * clamp(ubuf.innerShadow, 0.0, 1.0));
    }

    fragColor = finishColor(color, shapeAlpha);
}
