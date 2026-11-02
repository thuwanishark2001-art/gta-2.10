// Fixed map locations (Los Santos and surroundings). Coordinates are
// x, y, z[, angle]. Properties are created in game and live in the database.

enum E_POINT { Float:ptX, Float:ptY, Float:ptZ, Float:ptA }

// ATMs: /deposit, /withdraw, /balance
stock const gAtms[][E_POINT] = {
    { 1462.3, -1011.1, 26.8, 0.0 },     // Bank of Los Santos, Downtown
    { 1497.1, -1749.9, 15.4, 0.0 },     // Pershing Square
    { 1928.6, -1786.1, 13.5, 0.0 },     // Idlewood gas station
    { 2228.4, -1717.3, 13.5, 0.0 },     // Ganton
    { 1742.3, -1867.0, 13.6, 0.0 }      // Unity Station
};

// Gas stations: /refuel
stock const gGasStations[][E_POINT] = {
    { 1941.6, -1772.9, 13.6, 0.0 },     // Idlewood
    { 1004.0, -939.3, 42.2, 0.0 },      // Temple
    { 655.6, -565.0, 16.3, 0.0 },       // Dillimore
    { -90.9, -1169.4, 2.4, 0.0 },       // Flint County
    { 2202.0, 2474.0, 10.8, 0.0 }       // Las Venturas, for long trucking routes
};

// Dealership: Coutt and Schutz, Jefferson
stock const gDealer[E_POINT] = { 2131.8, -1150.5, 24.1, 0.0 };
stock const gDealerSpawn[E_POINT] = { 2148.6, -1138.6, 25.5, 270.0 };

// All Saints General: respawn after death
stock const gHospital[E_POINT] = { 1178.4, -1323.6, 14.1, 270.0 };

// LSPD: arrest point (garage), cells, release point
stock const gArrestPoint[E_POINT] = { 1568.6, -1694.8, 5.9, 0.0 };
#define KM_JAIL_INTERIOR        (6)
stock const gJailCell[E_POINT] = { 264.3, 77.6, 1001.0, 270.0 };
stock const gJailRelease[E_POINT] = { 1544.0, -1675.5, 13.6, 90.0 };

// ---- jobs --------------------------------------------------------------

// Trucker: Ocean Docks depot, truck spawn, delivery points
stock const gTruckDepot[E_POINT] = { 2197.0, -2662.9, 13.5, 0.0 };
stock const gTruckSpawn[E_POINT] = { 2213.0, -2650.0, 13.6, 270.0 };
stock const gTruckDrops[][E_POINT] = {
    { 236.0, -260.0, 1.6, 0.0 },        // Blueberry
    { 1338.0, 269.0, 19.6, 0.0 },       // Montgomery
    { 2318.0, 48.0, 26.5, 0.0 },        // Palomino Creek
    { -2109.0, -2433.0, 30.6, 0.0 },    // Angel Pine
    { 682.0, -470.0, 16.3, 0.0 },       // Dillimore
    { -205.0, 1105.0, 19.7, 0.0 },      // Fort Carson
    { 1990.0, -2380.0, 13.5, 0.0 },     // LS Airport cargo
    { -66.0, -1135.0, 1.1, 0.0 }        // Flint County
};

// Pizza: Idlewood Pizza Stack, bike spawn, houses
stock const gPizzaShop[E_POINT] = { 2105.0, -1806.5, 13.6, 0.0 };
stock const gPizzaSpawn[E_POINT] = { 2098.0, -1814.0, 13.4, 90.0 };
stock const gPizzaHouses[][E_POINT] = {
    { 2065.9, -1703.4, 14.1, 0.0 },
    { 2016.6, -1641.6, 13.8, 0.0 },
    { 2244.5, -1637.3, 15.9, 0.0 },
    { 2402.5, -1715.4, 14.1, 0.0 },
    { 2495.4, -1691.1, 14.8, 0.0 },
    { 1981.7, -1718.9, 17.0, 0.0 },
    { 2150.9, -1785.4, 13.5, 0.0 },
    { 2326.8, -1681.9, 14.9, 0.0 }
};

// Taxi: depot by Unity Station, fare stops around the city
stock const gTaxiDepot[E_POINT] = { 1777.0, -1932.0, 13.4, 0.0 };
stock const gTaxiSpawn[E_POINT] = { 1785.0, -1932.0, 13.4, 0.0 };
stock const gTaxiStops[][E_POINT] = {
    { 1481.0, -1740.0, 13.5, 0.0 },     // City Hall
    { 1172.0, -1323.0, 15.4, 0.0 },     // All Saints
    { 1543.0, -1675.0, 13.5, 0.0 },     // LSPD
    { 1835.0, -1682.0, 13.4, 0.0 },     // Alhambra
    { 2229.0, -1721.0, 13.5, 0.0 },     // Ganton gym
    { 1310.0, -1373.0, 13.5, 0.0 },     // Market
    { 1022.0, -1132.0, 23.8, 0.0 },     // Vinewood
    { 365.0, -2045.0, 7.7, 0.0 },       // Santa Maria Pier
    { 1685.0, -2240.0, 13.5, 0.0 },     // LS Airport
    { 2484.0, -1667.0, 13.3, 0.0 }      // Grove Street
};

// Bus: airport depot and a fixed loop through the city (taxi stops' curbs)
stock const gBusDepot[E_POINT] = { 1685.0, -2240.0, 13.5, 0.0 };
stock const gBusSpawn[E_POINT] = { 1685.0, -2240.0, 13.5, 0.0 };
stock const gBusRoute[][E_POINT] = {
    { 1481.0, -1740.0, 13.5, 0.0 },     // City Hall
    { 1310.0, -1373.0, 13.5, 0.0 },     // Market
    { 1022.0, -1132.0, 23.8, 0.0 },     // Vinewood
    { 1172.0, -1323.0, 15.4, 0.0 },     // All Saints
    { 1543.0, -1675.0, 13.5, 0.0 },     // LSPD
    { 1835.0, -1682.0, 13.4, 0.0 },     // Alhambra
    { 2229.0, -1721.0, 13.5, 0.0 },     // Ganton gym
    { 2484.0, -1667.0, 13.3, 0.0 }      // Grove Street
};

// Miner: Hunter Quarry, rock faces, ore drop-off
stock const gQuarry[E_POINT] = { 588.6, 868.5, -42.5, 0.0 };
stock const gQuarryRocks[][E_POINT] = {
    { 620.0, 850.0, -42.9, 0.0 },
    { 560.0, 900.0, -42.9, 0.0 },
    { 690.0, 880.0, -42.9, 0.0 },
    { 580.0, 940.0, -42.9, 0.0 },
    { 650.0, 920.0, -42.9, 0.0 }
};
stock const gQuarryDrop[E_POINT] = { 680.3, 826.4, -38.9, 0.0 };

// Mechanic: Idlewood garage and tow truck spawn
stock const gGarage[E_POINT] = { 2071.4, -1831.4, 13.5, 0.0 };
stock const gGarageSpawn[E_POINT] = { 2063.0, -1835.0, 13.6, 90.0 };

// ---- faction vehicles: faction id, model, position ---------------------

enum E_FACTION_CAR { fcFaction, fcModel, Float:fcX, Float:fcY, Float:fcZ, Float:fcA }

stock const gFactionCars[][E_FACTION_CAR] = {
    { 1, 596, 1602.0, -1683.9, 5.6, 90.0 },
    { 1, 596, 1602.0, -1688.0, 5.6, 90.0 },
    { 1, 596, 1602.0, -1692.0, 5.6, 90.0 },
    { 1, 596, 1602.0, -1696.0, 5.6, 90.0 },
    { 2, 416, 1178.0, -1338.0, 14.0, 270.0 },
    { 2, 416, 1178.0, -1308.0, 14.0, 270.0 },
    { 3, 409, 1478.0, -1737.0, 13.4, 270.0 }
};

stock bool:Place_Near(playerid, const point[E_POINT], Float:range)
{
    return IsPlayerInRangeOfPoint(playerid, range, point[ptX], point[ptY], point[ptZ]);
}

// ---- driving licence ----------------------------------------------------

// Licensing office by City Hall, the test car's spawn, and the test route.
stock const gLicenceOffice[E_POINT] = { 1490.0, -1771.0, 18.8, 0.0 };
stock const gLicenceCarSpawn[E_POINT] = { 1497.0, -1746.0, 13.4, 270.0 };
stock const gLicenceRoute[][E_POINT] = {
    { 1527.0, -1729.0, 13.4, 0.0 },
    { 1527.0, -1600.0, 13.4, 0.0 },
    { 1430.0, -1590.0, 13.4, 0.0 },
    { 1430.0, -1720.0, 13.4, 0.0 },
    { 1497.0, -1746.0, 13.4, 0.0 }
};
