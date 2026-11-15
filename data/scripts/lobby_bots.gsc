init()
{
    if ( !lobbybots( "active" ) )
        return;

    lobbybots_init_camos();
    level thread lobbybots_stop_autofill();
    level thread lobbybots_on_connect();
    level thread lobbybots_spawn_all();
    level thread lobbybots_on_game_ended();
}

lobbybots_stop_autofill()
{
    level endon( "game_ended" );

    for (;;)
    {
        level notify( "bot_connect_monitor" );
        wait 0.05;
    }
}

lobbybots_on_connect()
{
    level endon( "game_ended" );

    for (;;)
    {
        level waittill( "connected", player );

        if ( !isbot( player ) || !lobbybots( "isbot", player.name ) )
            continue;

        player.lobbybot = 1;

        team = lobbybots( "team", player.name );

        if ( level.teambased )
            team = lobbybots_balanced_team( player, team );

        if ( team != "" )
        {
            player.sessionteam = team;
            player.bot_team = team;
        }

        rank = lobbybots( "rank", player.name );
        prestige = lobbybots( "prestige", player.name );
        minxp = maps\mp\gametypes\_rank::getrankinfominxp( rank );
        maxxp = maps\mp\gametypes\_rank::getrankinfomaxxp( rank );
        rankxp = minxp;

        if ( maxxp > minxp )
            rankxp = randomintrange( minxp, maxxp );

        player.pers["bot_rank_recruit"] = rankxp;
        player.pers["bot_rank_regular"] = rankxp;
        player.pers["bot_rank_hardened"] = rankxp;
        player.pers["bot_rank_veteran"] = rankxp;
        player.pers["bot_rank_default"] = rankxp;
        player.pers["bot_prestige_recruit"] = prestige;
        player.pers["bot_prestige_regular"] = prestige;
        player.pers["bot_prestige_hardened"] = prestige;
        player.pers["bot_prestige_veteran"] = prestige;
        player.pers["bot_prestige_default"] = prestige;

        player lobbybots_apply_costume();
        player lobbybots_pick_camos();
    }
}

lobbybots_balanced_team( player, team )
{
    axis = 0;
    allies = 0;

    foreach ( other in level.players )
    {
        if ( other == player || !isdefined( other.sessionteam ) )
            continue;

        if ( other.sessionteam == "axis" )
            axis++;
        else if ( other.sessionteam == "allies" )
            allies++;
    }

    if ( axis < allies )
        return "axis";

    if ( allies < axis )
        return "allies";

    if ( team == "axis" || team == "allies" )
        return team;

    if ( randomint( 2 ) == 0 )
        return "axis";

    return "allies";
}

lobbybots_apply_costume()
{
    if ( maps\mp\_utility::practiceroundgame() )
    {
        lobbybots( "log", "practice round: game costume for " + self.name );
        return;
    }

    costume = lobbybots( "costume", self.name );

    if ( costume == "" )
        return;

    values = strtok( costume, " " );
    result = [];

    foreach ( value in values )
        result[result.size] = int( value );

    validated = validatecostume( result );

    text = "";

    foreach ( value in validated )
        text = text + value + " ";

    if ( !maps\mp\gametypes\_teams::validcostume( validated ) )
    {
        lobbybots( "log", "invalid costume for " + self.name + ": " + text );
        return;
    }

    self.costume = validated;
    self.sessioncostume = validated;
    self thread lobbybots_check_costume( validated );
    lobbybots( "log", "costume set for " + self.name + ": " + text );
}

lobbybots_check_costume( costume )
{
    self endon( "disconnect" );
    self waittill( "spawned_player" );
    wait 0.5;

    same = isdefined( self.costume ) && self.costume.size == costume.size;

    if ( same )
    {
        for ( i = 0; i < costume.size; i++ )
        {
            if ( self.costume[i] != costume[i] )
                same = 0;
        }
    }

    if ( !same )
        lobbybots( "log", "costume changed after spawn for " + self.name );
}

lobbybots_init_camos()
{
    level.lobbybots_camos = [];
    row = 0;

    for (;;)
    {
        name = tablelookupbyrow( "mp/camotable.csv", row, 1 );

        if ( !isdefined( name ) || name == "" )
            break;

        image = tablelookupbyrow( "mp/camotable.csv", row, 8 );
        hidden = int( tablelookupbyrow( "mp/camotable.csv", row, 11 ) );

        if ( isdefined( image ) && image != "" && !hidden && maps\mp\gametypes\_class::isvalidcamo( name ) )
            level.lobbybots_camos[level.lobbybots_camos.size] = name;

        row++;
    }
}

lobbybots_pick_camos()
{
    self.lobbybot_camo = "none";

    if ( level.lobbybots_camos.size > 0 && randomint( 100 ) < 75 )
        self.lobbybot_camo = common_scripts\utility::random( level.lobbybots_camos );
}

lobbybots_loadout_modify( loadout )
{
    if ( isdefined( level.lobbybots_prev_loadout_modify ) )
        loadout = self [[ level.lobbybots_prev_loadout_modify ]]( loadout );

    if ( isdefined( self.lobbybot ) && isdefined( self.lobbybot_camo ) && isdefined( loadout["loadoutPrimary"] ) && loadout["loadoutPrimary"] != "none" )
        loadout["loadoutPrimaryCamo"] = self.lobbybot_camo;

    return loadout;
}

lobbybots_hook_loadout()
{
    if ( isdefined( level.lobbybots_loadout_hooked ) || !isdefined( level.bot_funcs ) )
        return;

    level.lobbybots_loadout_hooked = 1;

    if ( isdefined( level.bot_funcs["gametype_loadout_modify"] ) )
        level.lobbybots_prev_loadout_modify = level.bot_funcs["gametype_loadout_modify"];

    level.bot_funcs["gametype_loadout_modify"] = ::lobbybots_loadout_modify;
}

lobbybots_spawn_all()
{
    level endon( "game_ended" );
    wait 0.5;

    lobbybots_hook_loadout();
    lobbybots( "precachecostumes" );
    count = lobbybots( "count" );

    for ( i = 0; i < count; i++ )
    {
        name = lobbybots( "name", i );

        if ( name == "" )
            continue;

        lobbybots( "setpending", name );
        bot = addbot( "", 0, 0, 0 );
        tries = 0;

        while ( !isdefined( bot ) && tries < 10 )
        {
            wait 1;
            tries++;
            lobbybots( "setpending", name );
            bot = addbot( "", 0, 0, 0 );
        }

        if ( !isdefined( bot ) )
            continue;

        print( "lobby_bots: " + name + " -> " + bot.name + "\n" );
        bot thread lobbybots_spawn_bot( lobbybots( "difficulty" ) );
        wait randomfloatrange( 0.1, 0.4 );
    }
}

lobbybots_spawn_bot( difficulty )
{
    self endon( "disconnect" );
    timeout = gettime() + 60000;

    while ( !self canspawntestclient() )
    {
        if ( gettime() >= timeout )
            return;

        wait 0.05;
    }

    self spawntestclient();
    self.equipment_enabled = 1;

    if ( difficulty != "" )
        maps\mp\bots\_bots_util::bot_set_difficulty( difficulty );

    self thread [[ level.bot_funcs["think"] ]]();
}

lobbybots_on_game_ended()
{
    level waittill( "game_ended" );

    foreach ( player in level.players )
    {
        if ( !isdefined( player.lobbybot ) )
            continue;

        score = 0;
        deaths = 0;

        if ( isdefined( player.score ) )
            score = player.score;

        if ( isdefined( player.deaths ) )
            deaths = player.deaths;

        lobbybots( "setstats", player.name, score, deaths );
    }
}
