const mat3 cubemapFaceToWorldMats[6] = {
    // positive X
    mat3( 0, 0,-1,
          0,-1, 0,
          1, 0, 0),
      // negative X
    mat3( 0, 0, 1,
          0,-1, 0,
         -1, 0, 0),
      // positive Y
    mat3( 1, 0, 0,
          0, 0, 1,
          0, 1, 0),
      // negative Y
    mat3( 1, 0, 0,
          0, 0,-1,
          0,-1, 0),
      // positive Z
    mat3( 1, 0, 0,
          0,-1, 0,
          0, 0, 1),
      // negative Z
    mat3(-1, 0, 0,
          0,-1, 0,
          0, 0,-1)
};

const mat3 worldToCubemapFaceMats[6] = {
    // positive X
    mat3( 0, 0, 1,
          0,-1, 0,
         -1, 0, 0),
      // negative X
    mat3( 0, 0,-1,
          0,-1, 0,
          1, 0, 0),
      // positive Y
    mat3( 1, 0, 0,
          0, 0, 1,
          0, 1, 0),
      // negative Y
    mat3( 1, 0, 0,
          0, 0,-1,
          0,-1, 0),
      // positive Z
    mat3( 1, 0, 0,
          0,-1, 0,
          0, 0, 1),
      // negative Z
    mat3(-1, 0, 0,
          0,-1, 0,
          0, 0,-1)
};

vec3 worldDirFromCubemapUV(vec2 uv, int face)
{
    vec3 faceDir = normalize(vec3(uv, 1));
    vec3 worldDir = cubemapFaceToWorldMats[face] * faceDir;

    return normalize(worldDir);
}
