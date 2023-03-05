const int hemisphere_hor_sizes[GRL_ZENITH_ANGLES+2] = {
	1,
	2,
	5,
	7,
	10,
	12,
	13,
	14,
	15,
	15,
	15,
	14,
	13,
	12,
	10,
	7,
	5,
	2,
	1
};
const int hemisphere_vec_count = 173;
const vec3 hemisphere_vecs[19][15] = {
	{
		vec3(0.000000, 1.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.000000, 0.984808, -0.173648),
		vec3(-0.000001, 0.984808, 0.173648),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.325280, 0.939693, 0.105690),
		vec3(0.201035, 0.939693, -0.276699),
		vec3(-0.201033, 0.939693, -0.276701),
		vec3(-0.325281, 0.939693, 0.105689),
		vec3(-0.000002, 0.939693, 0.342020),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.390915, 0.866026, 0.311745),
		vec3(0.487464, 0.866026, -0.111260),
		vec3(0.216943, 0.866026, -0.450484),
		vec3(-0.216940, 0.866026, -0.450485),
		vec3(-0.487463, 0.866026, -0.111262),
		vec3(-0.390917, 0.866026, 0.311743),
		vec3(-0.000002, 0.866026, 0.500000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.377821, 0.766045, 0.520026),
		vec3(0.611327, 0.766045, 0.198633),
		vec3(0.611327, 0.766045, -0.198631),
		vec3(0.377822, 0.766045, -0.520025),
		vec3(0.000002, 0.766045, -0.642787),
		vec3(-0.377819, 0.766045, -0.520027),
		vec3(-0.611326, 0.766045, -0.198634),
		vec3(-0.611328, 0.766045, 0.198630),
		vec3(-0.377823, 0.766045, 0.520024),
		vec3(-0.000003, 0.766045, 0.642787),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.383022, 0.642788, 0.663414),
		vec3(0.663413, 0.642788, 0.383023),
		vec3(0.766044, 0.642788, 0.000001),
		vec3(0.663414, 0.642788, -0.383021),
		vec3(0.383023, 0.642788, -0.663413),
		vec3(0.000002, 0.642788, -0.766044),
		vec3(-0.383020, 0.642788, -0.663415),
		vec3(-0.663412, 0.642788, -0.383024),
		vec3(-0.766044, 0.642788, -0.000003),
		vec3(-0.663415, 0.642788, 0.383019),
		vec3(-0.383025, 0.642788, 0.663412),
		vec3(-0.000004, 0.642788, 0.766044),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.402462, 0.500001, 0.766827),
		vec3(0.712724, 0.500001, 0.491959),
		vec3(0.859710, 0.500001, 0.104389),
		vec3(0.809748, 0.500001, -0.307095),
		vec3(0.574282, 0.500001, -0.648228),
		vec3(0.207255, 0.500001, -0.840859),
		vec3(-0.207251, 0.500001, -0.840860),
		vec3(-0.574279, 0.500001, -0.648231),
		vec3(-0.809746, 0.500001, -0.307100),
		vec3(-0.859711, 0.500001, 0.104384),
		vec3(-0.712727, 0.500001, 0.491955),
		vec3(-0.402466, 0.500001, 0.766825),
		vec3(-0.000005, 0.500001, 0.866025),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.407717, 0.342021, 0.846634),
		vec3(0.734681, 0.342021, 0.585889),
		vec3(0.916132, 0.342021, 0.209102),
		vec3(0.916133, 0.342021, -0.209100),
		vec3(0.734682, 0.342021, -0.585887),
		vec3(0.407719, 0.342021, -0.846633),
		vec3(0.000002, 0.342021, -0.939692),
		vec3(-0.407715, 0.342021, -0.846635),
		vec3(-0.734679, 0.342021, -0.585891),
		vec3(-0.916132, 0.342021, -0.209104),
		vec3(-0.916133, 0.342021, 0.209098),
		vec3(-0.734684, 0.342021, 0.585886),
		vec3(-0.407721, 0.342021, 0.846632),
		vec3(-0.000005, 0.342021, 0.939692),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.400557, 0.173649, 0.899667),
		vec3(0.731854, 0.173649, 0.658965),
		vec3(0.936607, 0.173649, 0.304323),
		vec3(0.979413, 0.173649, -0.102939),
		vec3(0.852869, 0.173649, -0.492402),
		vec3(0.578857, 0.173649, -0.796725),
		vec3(0.204755, 0.173649, -0.963287),
		vec3(-0.204750, 0.173649, -0.963288),
		vec3(-0.578853, 0.173649, -0.796728),
		vec3(-0.852867, 0.173649, -0.492407),
		vec3(-0.979412, 0.173649, -0.102944),
		vec3(-0.936609, 0.173649, 0.304318),
		vec3(-0.731858, 0.173649, 0.658961),
		vec3(-0.400562, 0.173649, 0.899664),
		vec3(-0.000005, 0.173649, 0.984807)
	}, {
		vec3(0.406736, 0.000001, 0.913546),
		vec3(0.743144, 0.000001, 0.669131),
		vec3(0.951056, 0.000001, 0.309018),
		vec3(0.994522, 0.000001, -0.104527),
		vec3(0.866026, 0.000001, -0.499998),
		vec3(0.587787, 0.000001, -0.809016),
		vec3(0.207914, 0.000001, -0.978147),
		vec3(-0.207909, 0.000001, -0.978148),
		vec3(-0.587783, 0.000001, -0.809019),
		vec3(-0.866024, 0.000001, -0.500003),
		vec3(-0.994521, 0.000001, -0.104532),
		vec3(-0.951058, 0.000001, 0.309013),
		vec3(-0.743148, 0.000001, 0.669127),
		vec3(-0.406741, 0.000001, 0.913543),
		vec3(-0.000005, 0.000001, 1.000000)
	}, {
		vec3(0.400557, -0.173647, 0.899667),
		vec3(0.731854, -0.173647, 0.658966),
		vec3(0.936608, -0.173647, 0.304323),
		vec3(0.979413, -0.173647, -0.102939),
		vec3(0.852870, -0.173647, -0.492402),
		vec3(0.578857, -0.173647, -0.796725),
		vec3(0.204755, -0.173647, -0.963287),
		vec3(-0.204750, -0.173647, -0.963288),
		vec3(-0.578853, -0.173647, -0.796728),
		vec3(-0.852867, -0.173647, -0.492407),
		vec3(-0.979413, -0.173647, -0.102944),
		vec3(-0.936609, -0.173647, 0.304318),
		vec3(-0.731858, -0.173647, 0.658962),
		vec3(-0.400562, -0.173647, 0.899665),
		vec3(-0.000005, -0.173647, 0.984808)
	}, {
		vec3(0.407717, -0.342019, 0.846634),
		vec3(0.734681, -0.342019, 0.585890),
		vec3(0.916133, -0.342019, 0.209102),
		vec3(0.916133, -0.342019, -0.209100),
		vec3(0.734683, -0.342019, -0.585888),
		vec3(0.407719, -0.342019, -0.846633),
		vec3(0.000002, -0.342019, -0.939693),
		vec3(-0.407715, -0.342019, -0.846636),
		vec3(-0.734680, -0.342019, -0.585892),
		vec3(-0.916133, -0.342019, -0.209105),
		vec3(-0.916134, -0.342019, 0.209098),
		vec3(-0.734684, -0.342019, 0.585886),
		vec3(-0.407722, -0.342019, 0.846633),
		vec3(-0.000005, -0.342019, 0.939693),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.402462, -0.499999, 0.766828),
		vec3(0.712725, -0.499999, 0.491959),
		vec3(0.859712, -0.499999, 0.104389),
		vec3(0.809749, -0.499999, -0.307096),
		vec3(0.574283, -0.499999, -0.648229),
		vec3(0.207256, -0.499999, -0.840861),
		vec3(-0.207251, -0.499999, -0.840862),
		vec3(-0.574279, -0.499999, -0.648232),
		vec3(-0.809747, -0.499999, -0.307100),
		vec3(-0.859712, -0.499999, 0.104384),
		vec3(-0.712728, -0.499999, 0.491956),
		vec3(-0.402466, -0.499999, 0.766826),
		vec3(-0.000005, -0.499999, 0.866026),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.383023, -0.642786, 0.663415),
		vec3(0.663415, -0.642786, 0.383023),
		vec3(0.766046, -0.642786, 0.000001),
		vec3(0.663416, -0.642786, -0.383022),
		vec3(0.383024, -0.642786, -0.663414),
		vec3(0.000002, -0.642786, -0.766046),
		vec3(-0.383021, -0.642786, -0.663416),
		vec3(-0.663414, -0.642786, -0.383025),
		vec3(-0.766046, -0.642786, -0.000003),
		vec3(-0.663417, -0.642786, 0.383020),
		vec3(-0.383026, -0.642786, 0.663413),
		vec3(-0.000004, -0.642786, 0.766046),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.377822, -0.766043, 0.520027),
		vec3(0.611328, -0.766043, 0.198633),
		vec3(0.611329, -0.766043, -0.198632),
		vec3(0.377823, -0.766043, -0.520026),
		vec3(0.000002, -0.766043, -0.642789),
		vec3(-0.377820, -0.766043, -0.520028),
		vec3(-0.611328, -0.766043, -0.198635),
		vec3(-0.611329, -0.766043, 0.198630),
		vec3(-0.377824, -0.766043, 0.520025),
		vec3(-0.000003, -0.766043, 0.642789),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.390917, -0.866024, 0.311746),
		vec3(0.487466, -0.866024, -0.111260),
		vec3(0.216944, -0.866024, -0.450486),
		vec3(-0.216941, -0.866024, -0.450487),
		vec3(-0.487465, -0.866024, -0.111263),
		vec3(-0.390918, -0.866024, 0.311744),
		vec3(-0.000002, -0.866024, 0.500002),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.325282, -0.939692, 0.105691),
		vec3(0.201036, -0.939692, -0.276701),
		vec3(-0.201035, -0.939692, -0.276702),
		vec3(-0.325283, -0.939692, 0.105689),
		vec3(-0.000002, -0.939692, 0.342022),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.000000, -0.984807, -0.173650),
		vec3(-0.000001, -0.984807, 0.173650),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}, {
		vec3(0.000000, -1.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000),
		vec3(0.000000, 0.000000, 0.000000)
	}
};