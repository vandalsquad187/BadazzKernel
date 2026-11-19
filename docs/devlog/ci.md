# CI (build-kernel.yml) — Archiv

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Vollstaendige CI-Mechanik: Matrix, miui-Resync, Config-Pfad, Artifact-Naming, release-Job.

## CI (`build-kernel.yml`)

- Trigger: push to `main` (full dual-build + release) / push to `miui/**` (artifact only, no release) / `workflow_dispatch`.
  Erste Live-Erkenntnis (19.10.): **Pushes via GITHUB_TOKEN feuern kein CI** — deshalb
  gibt es vom Auto-Resync-Push des Runners nie eine Trigger-Kaskade (das `[skip ci]`
  im Merge-Message-Text ist nur eine zweite Versicherungsstufe).
- **Dual-Build (seit 2026-10-19): 2 Builds pro Push** — Matrix:
  - `aosp` → ref `main` — für Stock/HOS/AOSP-ROMs (custom-USB-Profil aktiv)
  - `miui` → ref `miui/test` — für MIUI/HyperOS (stock-USB-Profil via `sweet_miui.config`-Merge + Branch-Reverts)
- **miui auto-resync**: der miui-Job mergt `origin/main` mit `git merge -X ours --no-edit`
  in `miui/test` und pusht zurück. Besonderheit: der GITHUB_TOKEN darf **keine
  `.github/`-Diffs pushen** ("refusing to allow ... workflow ... without `workflows` security"),
  und ein `workflows: write`-Scope ist in der Workflow-Syntax nicht gültig — deshalb
  wird nach dem Merge `.github/` per `git checkout HEAD~1 -- .github/` auf den
  Branch-Stand zurückgesetzt und merget per amend; der Push-Diff enthält dann keine
  Workflow-Datei und geht durch. Konflikte in den stock-blockierten USB-Dateien
  folgen dem MIUI-Profil;unksollte main-Änderungen fließen normal ein. Harte
  Merge-Fails brechen den Job sichtbar (manueller merge erforderlich).
- ccache-Keys pro Variante (`-aosp` / `-miui`) — untereinander nicht verdrängend.
- Config-Pfad: "Integrate NoMount"-Step (pinned `NOMOUNT_REF`, rendert `fs/nomount` frisch)
  → `make sweet_defconfig` (aosp) / `merge_config.sh sweet_defconfig sweet_miui.config`
  (miui) → inject `CONFIG_LOCALVERSION=…-build${run_number}-${variant}` → `olddefconfig`
  → validate required configs (`KSU`, `KSU_SUSFS`, `KSU_TAMPER_SYSCALL_TABLE`, `K6A_GOV`,
  `NOMOUNT`, `NFC_NQ_PN80T`, `PHY_QCOM_QUSB2`) → `make Image.gz dtb.img dtbo.img`
  → verify `CONFIG_PHY_QCOM_QUSB2` survived + `phy-qcom-qusb2.o` was built
  + `fs/nomount/nomount.o` built (NoMount built-in)
  → zips `Badazz-kernel-sweet-v${VERSION}-build${run_number}-aosp.zip` / `…-miui.zip`
  (inkl. `build-info.txt` mit `VARIANT=`) → beide als artifacts — ⚠️ Stolperstein:
  die *artifact-NAMES* müssen `${{ matrix.variant }}` enthalten, sonst haben beide
  Beine denselben Namen und `download-artifact@v4` lädt nur eins (Bug in build361,
  schief gefixt in build362).
- **Release** (separater Job `needs: [build]`, nur auf `main`): der `release`-Job
  lädt $# alle artifacts nach `dist/` (`merge-multiple: true`) und erzeugt via
  softprops **EIN Release** `v${VERSION}-badazz-build${run_number}` mit **beiden zips**
  (`files: dist/Badazz-kernel-sweet-*.zip`). Die Release-Body-Datei wird im Job
  selbst erzeugt (`body.md`, echter `git log`-Changelog) — nicht mehr als wörtliches
  `$(git log …)` im body-Template.
- Timings (warm): 15–20 min für beide Beine inkl. release; Build 353 (single) lag
  bei 18m9s. `gh run list` / `gh run watch <id>` to follow
