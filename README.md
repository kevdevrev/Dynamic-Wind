## Open Shaders integration

When Open Shaders exposes wind API revision 4, Dynamic Wind automatically uses its
spatial CPU wind field. Dynamic Wind continues to choose the weather wind target but
stops adding its own sine gust and angle turbulence. Animation receives local samples that include ambient wind, advected
gusts, and transient forces such as shouts and impacts. Physics handlers use the
same frame's physics velocity, which excludes effects that already push Havok
objects through native game physics.

Continuously driven object positions are sampled in one batch per update. Rotation is filtered as a vector
to avoid angle wrap and gust jitter, using only local base plus ambient-gust velocity;
all transient forces are excluded from continuous rotation. Restricted-angle objects
retain their original initial and new-target orientation updates. Visibility also uses only local base
plus ambient-gust velocity. Physics passes the horizontal strength and direction of
the Havok-excluded wind sample into Dynamic Wind's unchanged PushHandler and original
recursive `SetLinearImpulse` application path.
The Settings option **Enable Open Shaders Integration** appears only when a compatible
wind API is detected and defaults on. It controls the entire integration: turning it
off restores Dynamic Wind's own transitions, gusts, and original handler inputs;
turning it on uses Open Shaders samples and hands tree control to Open Shaders.
The option is persisted by Save Settings. While integration is enabled, Dynamic Wind
skips Tree Handler calls and disables its tree editing controls. It does not snapshot
or restore tree values; existing values remain when switching the integration on.
Without a compatible Open Shaders API,
Dynamic Wind uses its original procedural wind, handler inputs, and per-update
impulses. The added rotation filtering applies only to sampled Open Shaders wind.

Physics retains the original `min(1, windStrength^3) * windSensitivity` impulse
on every update, with no additional drag or time scaling. As in the original mod,
impulses are horizontal and their rate depends on the update frequency.
With gusts disabled, base wind still exerts steady pressure. Zero wind does not
instantly stop a swinging sign.

Model and base-object swaps retain their original target-driven behavior, including
with Open Shaders connected. They select configured variants when a new weather or
manual wind target arrives, or when a reference is first tracked on loading. They
use Open Shaders' local base plus ambient-gust velocity, excluding
all transient forces, and hold the chosen variant until the next event. New targets
are queued until a newer published field frame is observed after the Sky update;
repeated-frame samples keep the request pending, while failed sampling applies
Dynamic Wind's values immediately. A newer target supersedes an older pending request. They do not
change animation playback speed. If sampling is unavailable, they use the original
Dynamic Wind target values.
The separate animation handler follows the full local field using its configured
minimum/maximum speed mapping, with authored rates restored when control ends.

#### YOU NEED CMAKE < 3.5 !

#### WINDOWS ENVIRONMENT VARIABLES TO SET

1. **`COMMONLIB_SSE_FOLDER`**: The path to your clone of Commonlib.
2. **`VCPKG_ROOT`**: The path to your clone of [vcpkg](https://github.com/microsoft/vcpkg).
3. (optional) **`SKYRIM_FOLDER`**: path of your Skyrim Special Edition folder.
4. (optional) **`SKYRIM_MODS_FOLDER`**: path of the folder where your mods are.

#### THINGS TO EDIT

1. CMakeLists.txt
- **`AUTHORNAME`**
- **`MDDNAME`**
- (optional) Your plugin version. Default: `0.1.0.0`
2. vcpkg.json
- **`name`**: Your plugin's name.
- **`version-string`**: Your plugin version. Default: `0.1.0.0`

#### FEATURES
Automatically imports:
- [CLibUtil](https://github.com/powerof3/CLibUtil) by powerof3
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) by Thiago099
