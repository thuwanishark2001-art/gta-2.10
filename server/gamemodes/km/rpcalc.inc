// Pure game rules: pay, levels, fuel, plates. No server state, so these are
// unit tested directly in tests/km_tests.pwn.

stock Clamp(value, min, max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Paydays needed to go from `level` to `level + 1`.
stock Level_XpNeeded(level)
{
    return (level < 1 ? 1 : level) * 4;
}

// Bank interest for one payday, rounded down and capped.
stock Payday_Interest(bank)
{
    if (bank <= 0) return 0;
    new interest = bank / 1000 * KM_INTEREST_PERMILLE;
    return interest > KM_INTEREST_MAX ? KM_INTEREST_MAX : interest;
}

// Distance-based job pay: base + metres * perKm / 1000, capped at max.
stock Job_DistancePay(Float:metres, base, perKm, max)
{
    if (metres < 0.0) metres = 0.0;
    new pay = base + floatround(metres * float(perKm) / 1000.0, floatround_floor);
    return pay > max ? max : pay;
}

// Litres used in one fuel tick at the given speed (km/h).
stock Float:Fuel_Use(Float:speedKmh)
{
    if (speedKmh < 0.0) speedKmh = 0.0;
    return 0.05 + speedKmh * 0.002;
}

// Litres needed to fill up and what they cost.
stock Fuel_RefillCost(Float:fuel, &litres)
{
    litres = 100 - floatround(fuel, floatround_ceil);
    if (litres < 0) litres = 0;
    return litres * KM_FUEL_PRICE;
}

// KM-0001 from vehicle id 1. Ids above 9999 still fit: KM-12345.
stock Plate_Format(dest[], vehicleDbId, size = sizeof (dest))
{
    format(dest, size, "KM-%04d", vehicleDbId);
}

stock Float:Speed_Kmh(Float:vx, Float:vy, Float:vz)
{
    return floatsqroot(vx * vx + vy * vy + vz * vz) * 180.0;
}

stock Float:Dist3D(Float:x1, Float:y1, Float:z1, Float:x2, Float:y2, Float:z2)
{
    return floatsqroot((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2) + (z1 - z2) * (z1 - z2));
}

// Picks a random index other than `except`.
stock RandomOther(count, except)
{
    if (except < 0 || except >= count) return random(count);
    if (count < 2) return 0;
    new i = random(count - 1);
    return i >= except ? i + 1 : i;
}
