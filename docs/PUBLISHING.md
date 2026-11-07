# Publish SGK Hyperion to Open MPC

Nothing has been uploaded by this preparation. Proposed GitHub repo: SeanGoKrazy/SGK-Hyperion. If you choose another repo name, update Tools/package_release.py and catalog/plugins/sgk-hyperion.json before packaging. Keep UID 53474859, sgk_hyperion.so and catalog id sgk-hyperion stable.

1. Create that public repository on your own GitHub account. Upload the files from the source ZIP (the contents of SGK-Hyperion, not private backup or build directories). The root must contain LICENSE and CMakeLists.txt. No content catalog, Patches, Samples, Wavetables, vendor executables or license keys.
2. Install/play/check every UI page and Q-Link, save/reload a project, and uninstall using the exact standard release ZIP. This restarts MPC, so save projects first. Record the hardware and actual firmware in tested.json only after those checks. Reinstall if desired. Current installed private Hyperion was not changed for this packaging exercise.
3. Build with Tools/build-armv7.sh and use a compatible ARM sysroot. Or use the provided tested ARM candidate, with the source of the same version.
4. Use upstream https://github.com/open-mpc/mpc-vst-plugins tools at a pinned revision. The locally used tools revision was 0e1d64926dbe027568770f69bc31cd5b61f0ecf2. `python Tools/package_release.py --openmpc-tools /path/to/mpc-vst-plugins/tools --module build/armv7/sgk_hyperion.so --bench docs/validation/bench.json --out dist` calls their packager and catalog checker. The wrapper fixes Windows ZIP separators and adds accurate library/device notes; standard install.sh/uninstall.sh are not patched.
5. Once the exact ZIP passes your smoke test, make a GitHub release tagged v0.1.2, mark it Pre-release, and attach SGK-Hyperion-0.1.2-mpc-armv7.zip and its .sha256. Source is public in the repository; the source ZIP can be an extra release asset.
6. Open a pull request to open-mpc/mpc-vst-plugins adding catalog/plugins/sgk-hyperion.json. Copy only that registry file from this package. Its screenshot link points to the proposed v0.1.2 tag; it will not resolve until you publish that tag with the preview image. The image is labeled as a native-layout preview in our docs; replace it with a real MPC screen capture when available.
7. Catalog maintainers review and merge; acceptance is not automatic. Later releases are discovered from the same GitHub repo.

CPU notes must retain the WARN result and heavy-FM caveat. No aarch64/MPC OS 2.x or all-device compatibility claim. Repository/installer license is MIT; no third-party sound redistribution rights are granted. The Desktop private library backup must not be published.

Official workflow: https://open-mpc.github.io/workflow.html
