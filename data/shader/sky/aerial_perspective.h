#define AP_X 32
#define AP_Y 32
// 16 may be insufficient for out-of-atmosphere -> down, 32 looks well.
#define AP_Z 16

#define AP_STEPS_PER_CELL 10

#define AP_SETS_IMAGES 0
#define AP_TRANSMITTANCE_BINDING 0
#define AP_SCATTERING_BINDING 1

#define AP_SETS_CAM 1
#define AP_SETS_RATIO 2
#define AP_SETS_ENV0 3
#define AP_SETS_ENV1 4
#define AP_SETS_TRANSMITTANCE_SAMPLER0 5
#define AP_SETS_TRANSMITTANCE_SAMPLER1 6
#define AP_SETS_GATHERING_SAMPLER0 7
#define AP_SETS_GATHERING_SAMPLER1 8
#define AP_SETS_SUN 9
