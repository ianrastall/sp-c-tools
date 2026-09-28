/*
 * SGA report blocks, taken verbatim from Short_Games_Analyzer_V3.3.bat
 * (Short Games Analyzer Tool: idea and design (C) 2024, Stefan Pohl,
 * www.sp-cc.de). One block per engine and report; lines are in the order
 * the batch printed them after sorting. %name% is the batch variable of
 * that name, filled in by sga.c. Trailing spaces are significant (cmd kept
 * the blank that followed a redirection in the echoed line).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SPCT_SGA_TEMPLATES_H
#define SPCT_SGA_TEMPLATES_H

static const char *const SGA_FULL[28] = {
    "************************************************************************************************",
    "%engine%  All games: %numb_allgames% (%numb_wins% wins / %numb_draws% draws / %numb_losses% losses)",
    "*** Overall score: %engine_score%     Overall draw-rate: %engine_drawrate%",
    "*** Average game length: All: %length_all% moves (wins: %length_wins% moves / draws: %length_draws% moves / losses: %length_loss% moves) ",
    "*** Avg. eng play white: All: %length_all_w% moves (wins: %length_wins_w% moves / draws: %length_draws_w% moves / losses: %length_loss_w% moves) ",
    "*** Avg. eng play black: All: %length_all_b% moves (wins: %length_wins_b% moves / draws: %length_draws_b% moves / losses: %length_loss_b% moves) ",
    "************************************************************************************************",
    "*** all short wins   wins0-30   wins31-35  wins36-40  wins41-45  wins46-50  wins51-55  wins56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortwins%] =    [%list_wins_30mvs_pcnt%  +  %list_wins_35mvs_pcnt%  +  %list_wins_40mvs_pcnt%  +  %list_wins_45mvs_pcnt%  +  %list_wins_50mvs_pcnt%  +  %list_wins_55mvs_pcnt%  +  %list_wins_60mvs_pcnt%]",
    "***    [%list_all_wins%] =    [%list_wins_30mvs%  +  %list_wins_35mvs%  +  %list_wins_40mvs%  +  %list_wins_45mvs%  +  %list_wins_50mvs%  +  %list_wins_55mvs%  +  %list_wins_60mvs%]",
    "*** --------------------------------------------------------------------------------------------",
    "*** all short sacs  sacs queen   sacs 5+p   sacs 4p    sacs 3p    sacs 2p    sacs 1p(=pawn-unit) ",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%pcnt_sumsac%] =    [%pcnt_sac9%  +  %pcnt_sac5%  +  %pcnt_sac4%  +  %pcnt_sac3%  +  %pcnt_sac2%  +  %pcnt_sac1%]",
    "***    [%numb_sumsac%] =    [%numb_sac9%  +  %numb_sac5%  +  %numb_sac4%  +  %numb_sac3%  +  %numb_sac2%  +  %numb_sac1%]",
    "************************************************************************************************",
    "*** all short draws  draw0-30   draw31-35  draw36-40  draw41-45  draw46-50  draw51-55  draw56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortdraws%] =    [%list_draws_30mvs_pcnt%  +  %list_draws_35mvs_pcnt%  +  %list_draws_40mvs_pcnt%  +  %list_draws_45mvs_pcnt%  +  %list_draws_50mvs_pcnt%  +  %list_draws_55mvs_pcnt%  +  %list_draws_60mvs_pcnt%]",
    "***    [%list_all_draws%] =    [%list_draws_30mvs%  +  %list_draws_35mvs%  +  %list_draws_40mvs%  +  %list_draws_45mvs%  +  %list_draws_50mvs%  +  %list_draws_55mvs%  +  %list_draws_60mvs%]",
    "************************************************************************************************",
    "*** all short losses loss0-30   loss31-35  loss36-40  loss41-45  loss46-50  loss51-55  loss56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortlosses%] =    [%list_losses_30mvs_pcnt%  +  %list_losses_35mvs_pcnt%  +  %list_losses_40mvs_pcnt%  +  %list_losses_45mvs_pcnt%  +  %list_losses_50mvs_pcnt%  +  %list_losses_55mvs_pcnt%  +  %list_losses_60mvs_pcnt%]",
    "***    [%list_all_losses%] =    [%list_losses_30mvs%  +  %list_losses_35mvs%  +  %list_losses_40mvs%  +  %list_losses_45mvs%  +  %list_losses_50mvs%  +  %list_losses_55mvs%  +  %list_losses_60mvs%]",
    "************************************************************************************************",
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
};

static const char *const SGA_WINS[18] = {
    "************************************************************************************************",
    "%engine%  All games: %numb_allgames% (%numb_wins% wins / %numb_draws% draws / %numb_losses% losses)",
    "*** Overall score: %engine_score%     Overall draw-rate: %engine_drawrate% ",
    "*** Average game length: All: %length_all% moves (wins: %length_wins% moves / draws: %length_draws% moves / losses: %length_loss% moves) ",
    "*** Avg. eng play white: All: %length_all_w% moves (wins: %length_wins_w% moves / draws: %length_draws_w% moves / losses: %length_loss_w% moves) ",
    "*** Avg. eng play black: All: %length_all_b% moves (wins: %length_wins_b% moves / draws: %length_draws_b% moves / losses: %length_loss_b% moves) ",
    "************************************************************************************************",
    "*** all short wins   wins0-30   wins31-35  wins36-40  wins41-45  wins46-50  wins51-55  wins56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortwins%] =    [%list_wins_30mvs_pcnt%  +  %list_wins_35mvs_pcnt%  +  %list_wins_40mvs_pcnt%  +  %list_wins_45mvs_pcnt%  +  %list_wins_50mvs_pcnt%  +  %list_wins_55mvs_pcnt%  +  %list_wins_60mvs_pcnt%]",
    "***    [%list_all_wins%] =    [%list_wins_30mvs%  +  %list_wins_35mvs%  +  %list_wins_40mvs%  +  %list_wins_45mvs%  +  %list_wins_50mvs%  +  %list_wins_55mvs%  +  %list_wins_60mvs%]",
    "*** --------------------------------------------------------------------------------------------",
    "*** all short sacs  sacs queen   sacs 5+p   sacs 4p    sacs 3p    sacs 2p    sacs 1p(=pawn-unit) ",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%pcnt_sumsac%] =    [%pcnt_sac9%  +  %pcnt_sac5%  +  %pcnt_sac4%  +  %pcnt_sac3%  +  %pcnt_sac2%  +  %pcnt_sac1%]",
    "***    [%numb_sumsac%] =    [%numb_sac9%  +  %numb_sac5%  +  %numb_sac4%  +  %numb_sac3%  +  %numb_sac2%  +  %numb_sac1%]",
    "************************************************************************************************",
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
};

static const char *const SGA_DRAWS[13] = {
    "************************************************************************************************",
    "%engine%  All games: %numb_allgames% (%numb_wins% wins / %numb_draws% draws / %numb_losses% losses)",
    "*** Overall score: %engine_score%     Overall draw-rate: %engine_drawrate% ",
    "*** Average game length: All: %length_all% moves (wins: %length_wins% moves / draws: %length_draws% moves / losses: %length_loss% moves) ",
    "*** Avg. eng play white: All: %length_all_w% moves (wins: %length_wins_w% moves / draws: %length_draws_w% moves / losses: %length_loss_w% moves) ",
    "*** Avg. eng play black: All: %length_all_b% moves (wins: %length_wins_b% moves / draws: %length_draws_b% moves / losses: %length_loss_b% moves) ",
    "************************************************************************************************",
    "*** all short draws  draw0-30   draw31-35  draw36-40  draw41-45  draw46-50  draw51-55  draw56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortdraws%] =    [%list_draws_30mvs_pcnt%  +  %list_draws_35mvs_pcnt%  +  %list_draws_40mvs_pcnt%  +  %list_draws_45mvs_pcnt%  +  %list_draws_50mvs_pcnt%  +  %list_draws_55mvs_pcnt%  +  %list_draws_60mvs_pcnt%]",
    "***    [%list_all_draws%] =    [%list_draws_30mvs%  +  %list_draws_35mvs%  +  %list_draws_40mvs%  +  %list_draws_45mvs%  +  %list_draws_50mvs%  +  %list_draws_55mvs%  +  %list_draws_60mvs%]",
    "************************************************************************************************",
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
};

static const char *const SGA_LOSSES[13] = {
    "************************************************************************************************",
    "%engine%  All games: %numb_allgames% (%numb_wins% wins / %numb_draws% draws / %numb_losses% losses)",
    "*** Overall score: %engine_score%     Overall draw-rate: %engine_drawrate% ",
    "*** Average game length: All: %length_all% moves (wins: %length_wins% moves / draws: %length_draws% moves / losses: %length_loss% moves) ",
    "*** Avg. eng play white: All: %length_all_w% moves (wins: %length_wins_w% moves / draws: %length_draws_w% moves / losses: %length_loss_w% moves) ",
    "*** Avg. eng play black: All: %length_all_b% moves (wins: %length_wins_b% moves / draws: %length_draws_b% moves / losses: %length_loss_b% moves) ",
    "************************************************************************************************",
    "*** all short losses loss0-30   loss31-35  loss36-40  loss41-45  loss46-50  loss51-55  loss56-60",
    "*** --------------------------------------------------------------------------------------------",
    "***    [%percent_all_shortlosses%] =    [%list_losses_30mvs_pcnt%  +  %list_losses_35mvs_pcnt%  +  %list_losses_40mvs_pcnt%  +  %list_losses_45mvs_pcnt%  +  %list_losses_50mvs_pcnt%  +  %list_losses_55mvs_pcnt%  +  %list_losses_60mvs_pcnt%]",
    "***    [%list_all_losses%] =    [%list_losses_30mvs%  +  %list_losses_35mvs%  +  %list_losses_40mvs%  +  %list_losses_45mvs%  +  %list_losses_50mvs%  +  %list_losses_55mvs%  +  %list_losses_60mvs%]",
    "************************************************************************************************",
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
};

#endif /* SPCT_SGA_TEMPLATES_H */
