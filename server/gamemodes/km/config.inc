// Kingmakers RP gameplay settings. Server and database settings come from
// the environment (see scripts/entrypoint.sh).

#define KM_VERSION              "0.1.0"
#define KM_MODE_TEXT            "Kingmakers RP " KM_VERSION

// Accounts
#define KM_BCRYPT_COST          (12)
#define KM_LOGIN_TIMEOUT_MS     (120000)  // time allowed to log in
#define KM_MAX_LOGIN_ATTEMPTS   (3)       // wrong passwords per connection
#define KM_IP_FAIL_LIMIT        (10)      // wrong passwords per IP ...
#define KM_IP_FAIL_WINDOW_MIN   (15)      // ... within this many minutes
#define KM_PASSWORD_MIN         (6)
#define KM_PASSWORD_MAX         (64)

// Characters
#define KM_MAX_CHARACTERS       (3)
#define KM_START_CASH           (500)
#define KM_SPAWN_X              (1742.9)  // Unity Station, Los Santos
#define KM_SPAWN_Y              (-1861.4)
#define KM_SPAWN_Z              (13.58)
#define KM_SPAWN_A              (0.0)
#define KM_AUTOSAVE_MS          (300000)

// Anti-spam
#define KM_CHAT_WINDOW_MS       (3000)
#define KM_CHAT_MAX_IN_WINDOW   (3)
#define KM_CMD_WINDOW_MS        (3000)
#define KM_CMD_MAX_IN_WINDOW    (5)
#define KM_REPEAT_MS            (5000)    // same message again within this time
#define KM_SPAM_WARNINGS        (3)       // warnings before an automatic mute
#define KM_SPAM_MUTE_MIN        (2)

// Chat
#define KM_CHAT_RADIUS          (20.0)

// Admin levels
#define ADMIN_MOD               (1)       // kick, mute
#define ADMIN_ADMIN             (2)       // ban, unban
#define ADMIN_LEAD              (4)
#define ADMIN_OWNER             (5)       // set admin levels
