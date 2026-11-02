# Public releases: the signing key

Android only installs an update when it's signed with the **same key** as the
installed app. A public release needs one key, kept private and used for
every release from now on. If it's lost, players have to uninstall to
update, which deletes their settings.

The key never goes into the repository. It lives in two places:
- your own backup
- four GitHub repository secrets, which the **Release APK** workflow
  (`.github/workflows/release.yml`) uses to sign the APK

## 1. Make the key (once)

On your phone, in Termux (or on any computer with Java):

```
pkg install openjdk-17
keytool -genkeypair -v -keystore kingmakers-release.jks -alias kingmakers \
    -keyalg RSA -keysize 4096 -validity 10000
```

- It asks for a password. Pick a long one and write it down.
- For "Re-enter", type it again. If it asks for a key password, press
  Enter to use the same one.
- The name and organisation questions can be anything. "Kingmakers RP" is
  fine.

**Back up `kingmakers-release.jks` and the password now.** Copy the file
somewhere safe that isn't this phone, such as Google Drive or a USB stick:

```
termux-setup-storage
cp kingmakers-release.jks ~/storage/downloads/
```

Then upload it from Downloads to your Drive.

## 2. Give it to GitHub (once)

With the GitHub CLI in Termux, so you don't have to copy the long text by
hand:

```
pkg install gh
gh auth login
base64 -w0 kingmakers-release.jks | gh secret set KM_KEYSTORE_BASE64 -R thuwanishark2001-art/gta-2.10
gh secret set KM_KEYSTORE_PASSWORD -R thuwanishark2001-art/gta-2.10
gh secret set KM_KEY_ALIAS -R thuwanishark2001-art/gta-2.10 --body kingmakers
gh secret set KM_KEY_PASSWORD -R thuwanishark2001-art/gta-2.10
```

Notes on these commands:
- `gh auth login`: choose GitHub.com, HTTPS, and "Login with a web browser".
- **Password secrets:** the two password commands ask you to paste the
  value. Use the same password for both, unless you set a different key
  password.

To check, open the repository on github.com and go to **Settings > Secrets
and variables > Actions**. You should see the four names. The values stay
hidden.

## 3. Build a release

**From github.com:**
1. Open **Releases > Draft a new release**.
2. Type a new tag, for example `v1.0.0`.
3. Choose the branch with the code to release.
4. Click **Publish release**.

The Release APK workflow builds the APK, signs it and attaches it to that
release, usually within 20 minutes. Check its progress in the **Actions**
tab.

**From a computer with git:**

```
git tag v1.0.0
git push origin v1.0.0
```

That has the same result.

Once the workflow is on the default branch (`main`), you can also start it
from **Actions > Release APK > Run workflow**. That uploads the APK as a
download on the run page, without making a release.

Each build gets a higher version number, so it installs over the previous
release.

## Switching from test builds

Test builds sent during development are signed with a temporary debug key.
The first time you install a release build, uninstall the test build first.
After that, releases install over each other.

The release APK connects to the server set in `app/build.gradle`
(`kmServerHost`, currently `65.20.88.67`). If the server moves, change it
there before the next release.
