# Phase 5: Texture Integration - Implementation & Tracking

**Status:** 🟡 In Progress
**Estimated Time:** 30-45 minutes
**Actual Time:** TBD
**Started:** 2025-11-16 09:00:00
**Completed:** TBD
**Commit Hash:** TBD
**Previous Phase:** Phase 4 (81ae230) - Energy Conservation ✅

---

## Objectives

Enable texture-driven roughness and metallic parameters to replace uniform values. This allows materials to have spatially-varying surface properties from texture maps, enabling realistic variation across surfaces (scratches, wear, dirt, etc.).

**Key Goal:** Sample roughness and metallic from textures, apply tiling and strength factors, support packed ORM (Occlusion-Roughness-Metallic) textures using channel extraction.

---

## Task Breakdown

### Task 5.1: Add Roughness Texture Sampling
- [ ] **Status:** Pending
- **Location:** `shaders/pbr.glsl.frag`, after normal mapping (after line ~188)
- **Purpose:** Sample roughness from texture instead of using uniform value

**Current State (Phase 4):**
```glsl
/// Roughness used directly from uniform
float D = distributionGGX(NoH, material.roughness);
float G = geometrySmith(NoV, NoL, material.roughness);
```

**Add After Normal Mapping:**
```glsl
/// Sample roughness from texture or use uniform value
/// Roughness controls the size of specular highlights (smooth vs rough surfaces)
float roughness;
if (material.useRoughnessMap > 0.5) {
	/// Apply tiling to texture coordinates for roughness map
	vec2 roughnessTexCoord = fragTexCoord * material.roughnessTiling;

	/// Sample roughness texture and extract the appropriate channel
	/// Many roughness maps are single-channel (grayscale) stored in R, G, or B
	vec4 roughnessSample = texture(roughnessTexture, roughnessTexCoord);
	roughness = extractChannel(roughnessSample, material.roughnessChannel);

	/// Apply roughness strength factor to control the influence of the texture
	/// When strength is 0, we use the base material.roughness value
	/// When strength is 1, we use the full texture value
	roughness = mix(material.roughness, roughness, material.roughnessStrength);
} else {
	/// If no roughness map is enabled, use the uniform material value
	roughness = material.roughness;
}
```

**Explanation:**
- **Conditional sampling**: Only sample if `useRoughnessMap` is enabled
- **Tiling**: Apply `roughnessTiling` for texture repetition
- **Channel extraction**: Use `extractChannel()` for packed textures (e.g., ORM maps)
- **Strength blending**: Mix between uniform and texture values using `roughnessStrength`
- **Fallback**: Use uniform `material.roughness` if texture not enabled

**No placeholders used:** Complete roughness texture sampling implementation.

---

### Task 5.2: Add Metallic Texture Sampling
- [ ] **Status:** Pending
- **Location:** `shaders/pbr.glsl.frag`, after roughness sampling
- **Purpose:** Sample metallic from texture instead of using uniform value

**Add After Roughness Sampling:**
```glsl
/// Sample metallic from texture or use uniform value
/// Metallic determines if a surface is metal (1.0) or dielectric (0.0)
float metallic;
if (material.useMetallicMap > 0.5) {
	/// Apply tiling to texture coordinates for metallic map
	vec2 metallicTexCoord = fragTexCoord * material.metallicTiling;

	/// Sample metallic texture and extract the appropriate channel
	/// Metallic maps are typically single-channel, often packed with roughness
	vec4 metallicSample = texture(metallicTexture, metallicTexCoord);
	metallic = extractChannel(metallicSample, material.metallicChannel);

	/// Apply metallic strength factor
	/// Allows artists to modulate the texture values
	metallic = mix(material.metallic, metallic, material.metallicStrength);
} else {
	/// If no metallic map is enabled, use the uniform material value
	metallic = material.metallic;
}
```

**Explanation:**
- **Same pattern as roughness**: Consistent texture sampling approach
- **Channel extraction**: Enables ORM texture support (e.g., metallic in B channel)
- **Strength control**: Artists can dial texture influence up/down
- **Fallback**: Use uniform `material.metallic` if texture not enabled

**No placeholders used:** Complete metallic texture sampling implementation.

---

### Task 5.3: Replace Uniform References with Sampled Values
- [ ] **Status:** Pending
- **Location:** `shaders/pbr.glsl.frag`, in the lighting loop
- **Purpose:** Use sampled `roughness` and `metallic` variables instead of uniform values

**Current Code (Phase 4):**
```glsl
/// Calculate F0 (base reflectivity) based on metallic parameter
vec3 F0 = mix(vec3(0.04), albedo, material.metallic);

/// Cook-Torrance Specular BRDF
float D = distributionGGX(NoH, material.roughness);
vec3 F = fresnelSchlick(VoH, F0);
float G = geometrySmith(NoV, NoL, material.roughness);

/// Calculate kD (diffuse coefficient) for energy conservation
vec3 kD = (1.0 - F) * (1.0 - material.metallic);
```

**Replace With:**
```glsl
/// Calculate F0 (base reflectivity) based on sampled metallic value
/// Now uses texture-driven metallic for spatially-varying metal/dielectric behavior
vec3 F0 = mix(vec3(0.04), albedo, metallic);

/// Cook-Torrance Specular BRDF with sampled roughness
/// Roughness now varies across the surface based on the texture
float D = distributionGGX(NoH, roughness);
vec3 F = fresnelSchlick(VoH, F0);
float G = geometrySmith(NoV, NoL, roughness);

/// Calculate kD (diffuse coefficient) for energy conservation
/// kD now varies spatially with the metallic texture
vec3 kD = (1.0 - F) * (1.0 - metallic);
```

**Changes:**
- 3 occurrences: `material.roughness` → `roughness`
- 2 occurrences: `material.metallic` → `metallic`

**Impact:** Materials now have per-pixel variation in roughness and metallic properties.

**No placeholders used:** Direct variable replacement.

---

### Task 5.4: Build and Compile Shader
- [ ] **Status:** Pending
- **Command:** `cmake --build build`
- **Expected Output:**
  - Shader compiles successfully
  - SPIR-V size moderate increase (~300-500 bytes for texture sampling)
  - No syntax errors
  - Build completes cleanly

**Verification:**
1. Check for GLSL compilation errors
2. Verify SPIR-V updated
3. No CMake errors
4. SPIR-V size expected: ~11652 → ~12000 bytes (+300-500 bytes)

---

### Task 5.5: Run Application and Verify Texture Sampling
- [ ] **Status:** Pending
- **Command:** `cd build && timeout 10s ./LillUgsi || true`
- **Expected Behavior:**
  - Application launches successfully
  - **Spatially-varying roughness** (texture detail visible in specular)
  - **Spatially-varying metallic** (texture detail visible in material type)
  - Roughness texture affects highlight size across surface
  - Metallic texture affects metal/dielectric mix across surface
  - No crashes or validation errors

**Visual Checks:**
- [ ] Roughness texture visible (varying highlight sharpness)
- [ ] Metallic texture visible (varying metal/dielectric appearance)
- [ ] Specular highlights vary in size based on roughness map
- [ ] Material appearance varies based on metallic map
- [ ] Energy conservation still functional
- [ ] Normal mapping still functional
- [ ] Previous phases' features still working

**Log Checks:**
- [ ] No Vulkan validation errors
- [ ] Shader loads successfully
- [ ] Textures loaded correctly (roughness, metallic)
- [ ] No NaN or inf warnings
- [ ] No new warnings compared to Phase 4

---

### Task 5.6: Update This Tracking Document
- [ ] **Status:** Pending
- **Actions Required:**
  - Mark all tasks 5.1-5.5 as completed
  - Update status to 🟢 Completed
  - Fill in timestamps
  - Add commit hash
  - Document observations in Notes section
  - Update automated testing checklist
  - Note any issues encountered

---

## Implementation Summary

**Shader Changes:**
- File: `shaders/pbr.glsl.frag`
- Lines added: ~30-35 lines (roughness + metallic sampling + comments)
- Lines changed: 5 lines (variable replacements)
- New sampling: Roughness and metallic texture sampling with tiling
- Modified: All references to `material.roughness` and `material.metallic`

**Placeholders Used:** None - complete implementation

**Breaking Changes:** None, but visual output changes significantly (texture detail visible)

---

## Testing & Validation

### Automated Testing (Claude Verifies)

- [ ] ✅ Shader compiles without errors
- [ ] ✅ No GLSL syntax errors
- [ ] ✅ SPIR-V generation succeeds
- [ ] ✅ CMake build completes
- [ ] ✅ Application launches without crashes
- [ ] ✅ No Vulkan validation errors
- [ ] ✅ Roughness texture loads successfully
- [ ] ✅ Metallic texture loads successfully
- [ ] ✅ No NaN/inf values in output
- [ ] ✅ Framerate acceptable (>30 FPS)
- [ ] ✅ Clean shutdown

### Manual Testing (User Verifies)

- [ ] ✅ **Roughness texture detail visible across surface**
- [ ] ✅ **Metallic texture detail visible across surface**
- [ ] ✅ **Specular highlights vary in sharpness (roughness map working)**
- [ ] ✅ **Material type varies (metallic map working)**
- [ ] ✅ Energy conservation still functional
- [ ] ✅ Metallic workflow still functional
- [ ] ✅ Fresnel effect still visible
- [ ] ✅ Normal mapping still functional
- [ ] ✅ No visual artifacts (flickering, NaN black pixels)

### Texture Sampling Testing (User Verifies)

Test texture-driven material variation:

**Test 1: Roughness Map Variation**
- [ ] Smooth areas have sharp, concentrated specular highlights
- [ ] Rough areas have broad, dim specular highlights
- [ ] Roughness variation matches texture detail
- [ ] Roughness strength factor modulates texture influence

**Test 2: Metallic Map Variation**
- [ ] Metallic areas (value 1.0) show colored specular, no diffuse
- [ ] Dielectric areas (value 0.0) show white specular, balanced diffuse
- [ ] Metallic variation matches texture detail
- [ ] Metallic strength factor modulates texture influence

**Test 3: Combined Variation**
- [ ] Rough metallic areas: Broad colored highlights
- [ ] Smooth metallic areas: Sharp colored highlights
- [ ] Rough dielectric areas: Broad white highlights
- [ ] Smooth dielectric areas: Sharp white highlights

**Test 4: Tiling Factors**
- [ ] Changing `roughnessTiling` repeats roughness texture
- [ ] Changing `metallicTiling` repeats metallic texture
- [ ] Tiling independent from albedo tiling

---

## Expected Visual State

**Before Phase 5 (Phase 4):**
- Roughness uniform across entire surface (constant highlight size)
- Metallic uniform across entire surface (constant metal/dielectric)
- No spatial variation in material properties
- Energy conserved but visually flat

**After Phase 5:**
- **Roughness varies across surface** (texture-driven highlight variation)
- **Metallic varies across surface** (texture-driven material type)
- **Realistic wear patterns** (scratches, dirt, oxidation visible)
- **Spatial detail** in both diffuse and specular
- Energy still conserved, but now per-pixel based on texture values

**Visual Characteristics:**
- Roughness: **Spatially-varying specular sharpness**
- Metallic: **Spatially-varying metal/dielectric mix**
- Highlights: **Non-uniform across surface** (texture detail)
- Material type: **Non-uniform** (texture-driven variation)
- Overall appearance: **More realistic and detailed**

**Physical Accuracy Maintained:**
- Energy conservation: ✅ (still per-pixel, now texture-driven)
- Metallic workflow: ✅ (now texture-driven)
- Fresnel effect: ✅ (unchanged)
- Normal mapping: ✅ (unchanged)

**Known Limitations (Fixed Later):**
- Occlusion map not yet sampled → Phase 6 (optional)
- No IBL (Image-Based Lighting) → Future phase
- No HDR environment maps → Future phase

---

## Rollback Plan

If issues occur during Phase 5:

```bash
### Review changes
git diff HEAD shaders/pbr.glsl.frag

### Revert if needed
git checkout HEAD -- shaders/pbr.glsl.frag

### Rebuild
cmake --build build

### Test baseline (Phase 4 state)
cd build && timeout 5s ./LillUgsi || true
```

**Rollback Triggers:**
- Shader compilation fails
- Application crashes
- Black screen or visual corruption
- NaN/inf values causing flickering
- Textures not loading correctly
- Performance degradation >10%
- Previous phases' features broken

---

## Notes & Observations

### Implementation Notes

_To be filled during implementation._

**Expected process:**
1. Add roughness texture sampling after normal mapping
2. Add metallic texture sampling after roughness sampling
3. Replace 5 variable references in lighting loop
4. Build shader successfully
5. Application runs with texture detail visible

---

### Performance Notes

**Expected SPIR-V size increase:** ~300-500 bytes

**Expected performance impact:** Minimal
- Additional operations per fragment:
  - 2 texture samples (roughness, metallic)
  - 2 channel extractions (switch statements)
  - 2 mix operations (strength blending)
  - 2 conditional branches (useRoughnessMap, useMetallicMap)
- Total: ~6 additional operations per fragment
- Texture cache should be efficient (similar coordinates to albedo/normal)
- No additional function calls beyond `extractChannel()` (already exists)
- Expected framerate: Still >30 FPS

---

### Validation Errors

_To be documented during testing._

**Expected result:** Zero Vulkan validation errors

---

### Visual Observations

_User notes about texture detail, roughness variation, metallic variation, overall appearance._

**Expected observations:**
- Roughness texture clearly visible in specular highlights
- Metallic texture clearly visible in material appearance
- Realistic variation across surface
- Texture detail enhances realism significantly

---

### Texture Sampling Testing Results

_Document behavior with different tiling factors, strength factors, and texture combinations._

**Test scenarios:**
- Tiling factor adjustments
- Strength factor adjustments (0.0 → 1.0)
- Different roughness/metallic textures
- Combined ORM textures (if available)

---

## Next Phase Preparation

### Phase 6 Preview: Occlusion Map Integration (Optional)

Once Phase 5 is complete, Phase 6 could:
1. Enable texture-driven ambient occlusion (AO map sampling)
2. Apply AO to ambient lighting term
3. Test with full PBR texture sets (albedo + normal + ORM)
4. Validate channel extraction for packed ORM textures

**Expected outcome:** Realistic ambient shadowing in crevices and occluded areas.

### Prerequisites Completed
- [ ] Roughness texture sampling functional (pending user visual confirmation)
- [ ] Metallic texture sampling functional (pending user visual confirmation)
- [ ] Phase 5 validated and committed
- [ ] Tracking document updated
- [ ] No blocking issues from Phase 5

---

## References

**PBR Theory:**
- [LearnOpenGL PBR Theory](https://learnopengl.com/PBR/Theory)
- [Epic Games Real Shading in UE4](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)

**Texture Mapping:**
- Roughness maps: Grayscale, typically in R channel or combined in ORM
- Metallic maps: Grayscale, typically in B channel of ORM or separate
- ORM textures: Occlusion (R), Roughness (G), Metallic (B)
- Strength factors: Allow artist control over texture influence

**Channel Extraction:**
- `extractChannel(vec4, uint)`: Already implemented in Phase 1
- Enables support for packed textures
- Reduces texture memory usage

---

**Document Version:** 1.0
**Created:** 2025-11-16
**Updated By:** Claude Code
