// Tagged functions that are called before the module that defines them.
// Pawn needs the tag up front (warning 208 otherwise).

forward bool:Money_Give(playerid, cashDelta, bankDelta, const reason[], other = INVALID_PLAYER_ID, const details[] = "");
forward bool:Inv_Has(playerid, item, quantity = 1);
forward bool:Job_IsWorking(playerid, job);
forward bool:Prop_TryLock(playerid);
forward bool:Fac_Valid(id);
forward bool:Fac_IsOnDutyType(playerid, type);
