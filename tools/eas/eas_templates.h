/*
 * EAS report text, taken verbatim from EAS_Tool_V6.0.bat (Engine
 * Aggressiveness Statistics Tool: idea and design (C) 2025, Stefan Pohl,
 * www.sp-cc.de). %name% is the batch variable of that name, filled in by
 * eas.c. Trailing spaces are significant (cmd kept the blank before a
 * redirection, and one after it, in the echoed line).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SPCT_EAS_TEMPLATES_H
#define SPCT_EAS_TEMPLATES_H

static const char *const EAS_HEAD[] = {
    "***************************************************************************** ",
    "*** Engine Aggressiveness Tool V6.0 Score points Ratinglist ",
    "***************************************************************************** ",
    "*** Meanwhile, the scoring-system of the EAS-Tool got really complex, so ",
    "*** please check out the ReadMe-file, where you find the explanation... ",
    "***************************************************************************** ",
    "*** Evaluated file: %gamebase% ",
    "***************************************************************************** ",
    "                         early           bad  avg.win ",
    "Rank  EAS-Score  sacs    sacs   shorts  draws  moves  Engine/player ",
    "---------------------------------------------------------------------------",
    NULL
};

static const char *const EAS_AFTER1[] = {
    "-------------------------------------------------------------------",
    "*** Average length of all won games:            %avg_length_all_wins% moves",
    "*** Movelimit for early sac bonus  :             %earlysac_limit% moves",
    NULL
};

static const char *const EAS_MEDALS[] = {
    "********************************************************************************************* ",
    "********************************************************************************************* ",
    "********************************************************************************************* ",
    "*** EAS single-statistics (6 categories, each with Top5 engines): ",
    "********************************************************************************************* ",
    "A: Early sacrifices (percents of all sacs)           : [1]:%eas_A_goldmedal% ",
    "                                                       [2]:%eas_A_silvermedal% ",
    "                                                       [3]:%eas_A_bronzemedal% ",
    "                                                       [4]:%eas_A_fourthmedal% ",
    "                                                       [5]:%eas_A_fifthmedal% ",
    "B: Most sacrifices overall                           : [1]:%eas_B_goldmedal% ",
    "                                                       [2]:%eas_B_silvermedal% ",
    "                                                       [3]:%eas_B_bronzemedal% ",
    "                                                       [4]:%eas_B_fourthmedal% ",
    "                                                       [5]:%eas_B_fifthmedal% ",
    "C: Very short wins (%sh_level4% moves or less)                : [1]:%eas_C_goldmedal% ",
    "                                                       [2]:%eas_C_silvermedal% ",
    "                                                       [3]:%eas_C_bronzemedal% ",
    "                                                       [4]:%eas_C_fourthmedal% ",
    "                                                       [5]:%eas_C_fifthmedal% ",
    "D: Most short wins overall                           : [1]:%eas_D_goldmedal% ",
    "                                                       [2]:%eas_D_silvermedal% ",
    "                                                       [3]:%eas_D_bronzemedal% ",
    "                                                       [4]:%eas_D_fourthmedal% ",
    "                                                       [5]:%eas_D_fifthmedal% ",
    "E: Average length of all won games                   : [1]:%eas_E_goldmedal% ",
    "                                                       [2]:%eas_E_silvermedal% ",
    "                                                       [3]:%eas_E_bronzemedal% ",
    "                                                       [4]:%eas_E_fourthmedal% ",
    "                                                       [5]:%eas_E_fifthmedal% ",
    "F: Smallest number of bad draws                      : [1]:%eas_F_goldmedal% ",
    "                                                       [2]:%eas_F_silvermedal% ",
    "                                                       [3]:%eas_F_bronzemedal% ",
    "                                                       [4]:%eas_F_fourthmedal% ",
    "                                                       [5]:%eas_F_fifthmedal% ",
    "********************************************************************************************* ",
    "********************************************************************************************* ",
    NULL
};

static const char *const EAS_HEAD2[] = {
    "***************************************************************************** ",
    "***************************************************************************** ",
    "***************************************************************************** ",
    "*** 2nd Ratinglist with more stats in percent-values ************************ ",
    "***************************************************************************** ",
    "*** Average length of all won games                  :%avg_length_all_wins% moves ",
    "*** Calculated limit for short wins giving EAS-points: %shortwin_movelimit% moves ",
    "*** Movelimit for early sac bonus                    : %earlysac_limit% moves ",
    "***************************************************************************** ",
    "                       avg.win                                                               early                                                            bad   ",
    "Rank  EAS-Score   wins  moves   sacs    sacsQ    sacs5+   sacs4    sacs3    sacs2    sacs1   sacs   all shorts short%sh_level5%  short%sh_level4%  short%sh_level3%  short%sh_level2%  short%sh_level1%   draws    Engine/player",
    "-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------",
    NULL
};

static const char *const EAS_HEAD3[] = {
    "***************************************************************************** ",
    "***************************************************************************** ",
    "***************************************************************************** ",
    "*** 3rd Ratinglist, showing EAS-points instead of percents ****************** ",
    "***************************************************************************** ",
    "*** (Mention, 5000 EAS-points are added to early sac bonus, for each move,*** ",
    "*** the average length of won games of the engine is shorter than the     *** ",
    "*** average length of all wins in the source.pgn.)                        *** ",
    "***************************************************************************** ",
    "***************************************************************************** ",
    "                             early              bad ",
    "Rank  EAS-Score      sacs     sacs   shorts    draws    Engine/player ",
    "-------------------------------------------------------------------------------------",
    NULL
};

static const char *const EAS_TAIL[] = {
    "******************************************************************************************** ",
    "******************************************************************************************** ",
    "**************************************************** ",
    "*** EAS-Tool (C) 2025 Stefan Pohl (www.sp-cc.de) *** ",
    "**************************************************** ",
    NULL
};

/* One line per engine in each work file, as echoed by the batch. */
static const char EAS_W1_WARN[] = " %engine_eas%  %percent_all_sacs%  %earlysacs_percent%  %percent_all_shorts%  %percent_bad_draws%  %avg_length_eng_wins%   %engine%   XXXXX WARNING: Not enough games, EAS-score not reliable [50+ wins and 30+ draws needed] XXXXX ";
static const char EAS_W1[] = " %engine_eas%  %percent_all_sacs%  %earlysacs_percent%  %percent_all_shorts%  %percent_bad_draws%  %avg_length_eng_wins%   %engine% ";
static const char EAS_W2[] = " %engine_eas%   %winform%  %avg_length_eng_wins%   %percent_all_sacs% =[%perc_sac9% + %perc_sac5% + %perc_sac4% + %perc_sac3% + %perc_sac2% + %perc_sac1%] %earlysacs_percent%   %percent_all_shorts% = [%perc_40mvs% + %perc_45mvs% + %perc_50mvs% + %perc_55mvs% + %perc_60mvs%]  %percent_bad_draws%   %engine% ";
static const char EAS_W3[] = " %engine_eas%    %eas_sacs%  %eas_earlysacs%  %eas_short_wins%  %eas_bad_draws%    %engine% ";
static const char EAS_SS_E[] = "%ma_moveaverage% %engine% ";
static const char EAS_SS_F[] = "%percent% %engine% ";
static const char EAS_SS_C[] = "%percent% %engine% ";
static const char EAS_SS_D[] = "%percent% %engine% ";
static const char EAS_SS_A[] = "%percent% %engine% ";
static const char EAS_SS_B[] = "%percent% %engine% ";

#endif /* SPCT_EAS_TEMPLATES_H */
