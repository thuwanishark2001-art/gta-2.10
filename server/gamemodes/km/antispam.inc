// Flood and repeat-message limiter. Pure logic over a small state array so
// it can be unit tested; gamemode code keeps one state per player and kind.

enum E_SPAM
{
    SPAM_WINDOW_START,  // tick when the current window began
    SPAM_COUNT,         // messages in the current window
    SPAM_LAST_HASH,     // hash of the last message
    SPAM_LAST_TICK      // tick of the last message
}

enum E_SPAM_RESULT
{
    SPAM_OK,
    SPAM_FLOOD,
    SPAM_REPEAT
}

stock Spam_Reset(data[E_SPAM])
{
    data[SPAM_WINDOW_START] = 0;
    data[SPAM_COUNT] = 0;
    data[SPAM_LAST_HASH] = 0;
    data[SPAM_LAST_TICK] = 0;
}

// Records one message at tick `now` and says whether it is allowed.
// Blocked messages still count, so holding the button keeps you blocked.
stock E_SPAM_RESULT:Spam_Check(data[E_SPAM], now, const text[], windowMs, maxInWindow, repeatMs)
{
    if (data[SPAM_COUNT] == 0 || now - data[SPAM_WINDOW_START] >= windowMs)
    {
        data[SPAM_WINDOW_START] = now;
        data[SPAM_COUNT] = 0;
    }
    data[SPAM_COUNT]++;

    new hash = StrHashCI(text);
    new bool:repeat = (data[SPAM_LAST_TICK] != 0 && hash == data[SPAM_LAST_HASH] && now - data[SPAM_LAST_TICK] < repeatMs);
    data[SPAM_LAST_HASH] = hash;
    data[SPAM_LAST_TICK] = now;

    if (data[SPAM_COUNT] > maxInWindow) return SPAM_FLOOD;
    if (repeat) return SPAM_REPEAT;
    return SPAM_OK;
}
