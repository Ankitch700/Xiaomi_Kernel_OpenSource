#ifndef __HQSYS_PCBA_
#define __HQSYS_PCBA_

//N6 code for HQ-301812 by hugaojian at 20230713 start
/* N6R code for HQ-429130 by jishen at 2024/10/24 start */
typedef enum
{
	PCBA_INFO_UNKNOW = 0,

	PCBA_N6_P0_1_GLOBAL,
	PCBA_N6_P0_1_GLOBAL_P,
	PCBA_N6R_P0_1_GLOBAL,

	PCBA_N6_P1_GLOBAL,
	PCBA_N6_P1_GLOBAL_P,
	PCBA_N6R_P1_GLOBAL,

	PCBA_N6_P1_1_GLOBAL,
	PCBA_N6_P1_1_GLOBAL_P,
	PCBA_N6R_P1_1_GLOBAL,

	PCBA_N6_P2_GLOBAL,
	PCBA_N6_P2_GLOBAL_P,
	PCBA_N6R_P2_GLOBAL,

	PCBA_N6_MP_GLOBAL,
	PCBA_N6_MP_GLOBAL_P,
	PCBA_N6R_MP_GLOBAL,

	PCBA_INFO_END,
} PCBA_INFO;
/* N6R code for HQ-429130 by jishen at 2024/10/24 end */
//N6 code for HQ-301812 by hugaojian at 20230713 end

typedef enum
{
	STAGE_UNKNOW = 0,
	P0_1,
	P1,
	P1_1,
	P2,
	MP,
} PROJECT_STAGE;

struct project_stage {
	int voltage_min;
	int voltage_max;
	PROJECT_STAGE project_stage;
	char hwc_level[20];
} stage_map[] = {
	{ 130,  225,   P0_1,   "P0.1",},
	{ 226,  315,   P1,     "P1",  },
	{ 316,  405,   P1_1,   "P1.1",},
	{ 406,  485,   P2,     "P2",  },
	{ 486,  565,   MP,     "MP",  },
};

//N6 code for HQ-301812 by hugaojian at 20230713 start
/* N6R code for HQ-429130 by jishen at 2024/10/24 start */
struct pcba {
	PCBA_INFO pcba_info;
	char pcba_info_name[32];
} pcba_map[] = {
	{PCBA_N6_P0_1_GLOBAL,        "PCBA_N6_P0-1_GLOBAL"},
	{PCBA_N6_P0_1_GLOBAL_P,      "PCBA_N6_P0-1_GLOBAL_P"},
	{PCBA_N6R_P0_1_GLOBAL,      "PCBA_N6R_P0-1_GLOBAL"},

	{PCBA_N6_P1_GLOBAL,          "PCBA_N6_P1_GLOBAL"},
	{PCBA_N6_P1_GLOBAL_P,        "PCBA_N6_P1_GLOBAL_P"},
	{PCBA_N6R_P1_GLOBAL,        "PCBA_N6R_P1_GLOBAL"},

	{PCBA_N6_P1_1_GLOBAL,        "PCBA_N6_P1-1_GLOBAL"},
	{PCBA_N6_P1_1_GLOBAL_P,      "PCBA_N6_P1-1_GLOBAL_P"},
	{PCBA_N6R_P1_1_GLOBAL,      "PCBA_N6R_P1-1_GLOBAL"},

	{PCBA_N6_P2_GLOBAL,        "PCBA_N6_P2_GLOBAL"},
	{PCBA_N6_P2_GLOBAL_P,      "PCBA_N6_P2_GLOBAL_P"},
	{PCBA_N6R_P2_GLOBAL,      "PCBA_N6R_P1-2_GLOBAL"},

	{PCBA_N6_MP_GLOBAL,          "PCBA_N6_MP_GLOBAL"},
	{PCBA_N6_MP_GLOBAL_P,        "PCBA_N6_MP_GLOBAL_P"},
	{PCBA_N6R_MP_GLOBAL,        "PCBA_N6R_MP_GLOBAL"},
};
/* N6R code for HQ-429130 by jishen at 2024/10/24 end */
//N6 code for HQ-301812 by hugaojian at 20230713 end

struct PCBA_MSG {
	PCBA_INFO huaqin_pcba_config;
	PROJECT_STAGE pcba_stage;
	unsigned int pcba_config;
	unsigned int pcba_config_count;
	const char *rsc;
	const char *sku;
};

//N6 code for HQ-301812 by hugaojian at 20230713 start
struct PCBA_MSG* get_pcba_msg(void);
//N6 code for HQ-301812 by hugaojian at 20230713 end

#endif
