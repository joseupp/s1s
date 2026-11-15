init()
{
    if ( getdvar( "mapname" ) != "mp_vlobby_room" )
        return;

    level thread vlbots_hook_party_members();
}

vlbots_hook_party_members()
{
    for ( i = 0; i < 400 && !isdefined( level.partymembers_cb ); i++ )
        wait 0.05;

    level.vlbots_weapons = [];
    level.partymembers_cb = ::vlbots_party_members;
}

vlbots_party_members( members )
{
    foreach ( member in members )
    {
        if ( !isdefined( member.player_controller ) || member.player_controller >= 0 || member.primary < 0 )
            continue;

        vlbots_set_weapon( member );
    }

    maps\mp\_vl_camera::party_members( members );
}

vlbots_set_weapon( member )
{
    key = "" + member.xuid;

    if ( !isdefined( level.vlbots_weapons[key] ) )
        level.vlbots_weapons[key] = vlbots_random_primary();

    attachment_none = int( tablelookup( "mp/attachmenttable.csv", 3, "none", 0 ) );

    member.primary = level.vlbots_weapons[key];

    if ( isdefined( member.name ) )
        setdvar( "vlbot_wpn_" + member.name, level.vlbots_weapons[key] );

    member.primaryattachment1 = attachment_none;
    member.primaryattachment2 = attachment_none;
    member.primaryattachment3 = attachment_none;
    member.primarycamo = int( tablelookup( "mp/camoTable.csv", 1, "none", 0 ) );
    member.primaryreticle = int( tablelookup( "mp/reticleTable.csv", 1, "none", 0 ) );
}

vlbots_random_primary()
{
    weapons = [];
    weapons[weapons.size] = "iw5_ak12";
    weapons[weapons.size] = "iw5_bal27";
    weapons[weapons.size] = "iw5_hbra3";
    weapons[weapons.size] = "iw5_himar";
    weapons[weapons.size] = "iw5_arx160";
    weapons[weapons.size] = "iw5_m182spr";
    weapons[weapons.size] = "iw5_mp11";
    weapons[weapons.size] = "iw5_sn6";
    weapons[weapons.size] = "iw5_hmr9";
    weapons[weapons.size] = "iw5_sac3";
    weapons[weapons.size] = "iw5_asm1";
    weapons[weapons.size] = "iw5_kf5";
    weapons[weapons.size] = "iw5_lsat";
    weapons[weapons.size] = "iw5_em1";
    weapons[weapons.size] = "iw5_asaw";
    weapons[weapons.size] = "iw5_epm3";
    weapons[weapons.size] = "iw5_maul";
    weapons[weapons.size] = "iw5_uts19";
    weapons[weapons.size] = "iw5_rhino";
    weapons[weapons.size] = "iw5_mors";
    weapons[weapons.size] = "iw5_gm6";
    weapons[weapons.size] = "iw5_thor";
    weapons[weapons.size] = "iw5_m990";

    for ( tries = 0; tries < 10; tries++ )
    {
        name = weapons[randomint( weapons.size )];
        id = tablelookup( "mp/statstable.csv", 4, name, 0 );

        if ( id != "" )
            return int( id );
    }

    return int( tablelookup( "mp/statstable.csv", 4, "iw5_ak12", 0 ) );
}
