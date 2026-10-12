-- TPS gun (vnum 5301 "TPS Carbine"): clone of Battle Bow+0 (2030) so the shot
-- types 60/61 have a real weapon in inventory, and it lands in the first test
-- character's bag. Applied to the sqlite seed in make-server-pack.sh.

INSERT OR IGNORE INTO item_proto
    (vnum, name, locale_name, type, subtype, weight, size, antiflag, flag,
     wearflag, immuneflag, gold, shop_buy_price, refined_vnum, refine_set,
     refine_set2, magic_pct, limittype0, limitvalue0, limittype1, limitvalue1,
     applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2,
     value0, value1, value2, value3, value4, value5,
     socket0, socket1, socket2, socket3, socket4, socket5,
     specular, socket_pct, addon_type)
-- antiflag 312 like a normal weapon: the bow clone's 52 bans warrior/sura/
-- shaman (incl. the seeded test shaman) so the server refused the equip.
SELECT 5301, 'carbine', 'TPS Carbine', type, subtype, weight, size, 312, flag,
     wearflag, immuneflag, gold, shop_buy_price, 0, 0,
     0, magic_pct, limittype0, limitvalue0, limittype1, limitvalue1,
     applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2,
     value0, value1, value2, value3, value4, value5,
     socket0, socket1, socket2, socket3, socket4, socket5,
     specular, socket_pct, addon_type
FROM item_proto WHERE vnum = 2030;

-- The starter carbine goes to the first character of the seeded test account;
-- account lives in a separate sqlite, so make-server-pack.sh resolves the
-- player id and does the item INSERT itself.
