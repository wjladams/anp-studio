# Google OAuth for ANP Studio (Forms)

Live Google Forms create/import uses a **Desktop** OAuth client in your Google
Cloud project. Personal Gmail accounts work. Microsoft Forms is not supported
yet.

Excel and CSV import/export do **not** require Google credentials.

## 1. Create a Google Cloud project

1. Open [Google Cloud Console](https://console.cloud.google.com/) and sign in
   with your Gmail (or Workspace) account.
2. Create a project (e.g. `anp-studio`) or select an existing one.

## 2. Enable APIs

In **APIs & Services → Library**, enable:

- [Google Forms API](https://console.cloud.google.com/apis/library/forms.googleapis.com)
- [Google Drive API](https://console.cloud.google.com/apis/library/drive.googleapis.com)
  (needed if form create/share uses Drive file operations)

## 3. Configure the OAuth consent screen

1. Open **Google Auth platform** (Branding / Audience / Data Access), or
   **APIs & Services → OAuth consent screen**.
2. App name: `ANP Studio` (or similar).
3. User support and developer contact: your email.
4. Audience: **External** for `@gmail.com` (Internal is Workspace-only).
5. While the app is in **Testing**, add your Gmail under **Test users**.
   Connect will fail for accounts not listed.
6. Add the minimum Forms scopes required by ANP Studio (documented with the
   Forms integration code when enabled). Prefer the smallest scope set.

Solo/dev use can stay in Testing. Publishing to Production is only needed for
arbitrary public users.

## 4. Create a Desktop OAuth client

1. **Google Auth platform → Clients → Create client**.
2. Application type: **Desktop app**.
3. Name: e.g. `ANP Studio Desktop`.
4. Create and download / copy the **Client ID** and **Client secret**.
   Store the secret securely; it may only be shown once.

Official walkthrough (consent + Desktop client):
[Forms API quickstart](https://developers.google.com/workspace/forms/api/quickstart/python).

## 5. Wire credentials into the build (recommended: `.env`)

Do **not** commit client secrets to git. `.env` is gitignored.

### Easiest path

1. Copy the example file and edit:

```bash
cp .env.example .env
```

2. Put your Desktop client values in `.env` (either naming style works):

```bash
# Preferred
ANPSTUDIO_GOOGLE_CLIENT_ID=123456789-xxxx.apps.googleusercontent.com
ANPSTUDIO_GOOGLE_CLIENT_SECRET=GOCSPX-xxxxxxxx

# Also accepted
# GOOGLE_CLIENT_ID=...
# GOOGLE_CLIENT_SECRET=...
```

3. Reconfigure and build (CMake reads `.env` at configure time):

```bash
cmake -S . -B build -DANPSTUDIO_BUILD_GUI=ON
cmake --build build --target anpstudio
```

You should see: `Google OAuth: client credentials loaded (Connect will be available)`.

CMake generates `build/gui/google_oauth_config.hpp` (inside the ignored build
tree) with the values. App code includes that header via
`anpstudio::oauth::kGoogleClientId` / `kGoogleClientSecret` /
`kGoogleOAuthConfigured`.

If you change `.env`, run `cmake -S . -B build ...` again (or delete the
generated header) so configure picks up the new values.

### Alternatives

| Mode | How |
|------|-----|
| CMake `-D` | `-DANPSTUDIO_GOOGLE_CLIENT_ID=…` `-DANPSTUDIO_GOOGLE_CLIENT_SECRET=…` |
| Environment | Export `ANPSTUDIO_GOOGLE_CLIENT_ID` / `ANPSTUDIO_GOOGLE_CLIENT_SECRET` (or `GOOGLE_*`) before `cmake` |

Priority: existing CMake `-D` / cache → `.env` → process environment.

If credentials are missing, configure prints `Google OAuth: not configured` and
`kGoogleOAuthConfigured` is false. Excel/CSV still work.

### Safety checklist

- Keep `.env` out of git (already in `.gitignore`)
- Never paste secrets into issues, chat, or committed docs
- If a secret was exposed, **rotate it** in Cloud Console (Clients → your
  Desktop client → reset secret) and update `.env`
- Release CI should inject secrets via the runner’s secret store, not a checked-in file

## 6. Connect in the app

1. Build ANP Studio with credentials configured (step 5).
2. Run the app, then **File → Settings… → Connected accounts**.
3. Click **Connect…** — your browser opens Google’s consent screen.
4. Approve access; the browser shows “Google account connected” and you can
   return to ANP Studio. Status should show your email.
5. Tokens are stored in Qt `QSettings` under `ConnectedAccounts/Google` (per
   user on this machine), not in model JSON.
6. **Disconnect** clears stored tokens.

## 7. Create and import Google Forms

With a Google account connected and a model open:

1. **Collect judgments… → Create form…** — builds a form from pairwise/ratings
   judgments, stores the link on the model (`google_forms`), and opens it in
   the browser. Respondents see plain-English questions (same wording as Excel
   templates), section breaks, and a short Saaty legend — **not** raw
   `[anp:…]` tags. The link stores a **questionId → tag** map for import, plus
   a **structure fingerprint** (hash of judgment questions / scales at create
   time). Legacy forms that still embed tags in titles continue to import.
   Open via **Participants → Collect judgments…** or Session → **Collect…**.
2. Collect responses in Google Forms (respondents enter name and optionally
   email).
3. **Collect judgments… → Import results…** — pulls responses for the
   latest linked form. Tags are resolved from the stored questionId map when
   present; otherwise from `[anp:…]` in the question title. Matching is by
   email first, then name (case-insensitive). Missing participants are
   **auto-created**, then their judgments are imported. A summary dialog
   reports how many participants were created and judgments set.

### Structure drift (out-of-date forms)

Changing nodes, connections, alternatives, or rating scales changes the
fingerprint. Judgment *values* and participant roster edits do **not**.

- Menu items show **(out of date)** when the latest linked form no longer
  matches.
- **Import** warns and offers **Import matching only** (applies answers whose
  `[anp:]` tags still exist) or Cancel.
- **Create Google Form…** again archives the previous link on the model and
  pins a new fingerprint. Prefer a new form after structural edits rather than
  editing the live Google Form by hand.

Legacy links without a fingerprint are treated as out of date.

## Smoke test

Connect → create a throwaway form from a multi-user sample → submit as a new
name/email → **Import Google Form results…** → confirm the new participant and
judgments appear → rename a node → menu shows out of date → import matching
only or create a new form → Disconnect clears stored tokens.
