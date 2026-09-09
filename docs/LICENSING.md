# Commercial use and attribution

The current project-owned source, models, and documentation are licensed under
the [Apache License 2.0](../LICENSE). You may use them in commercial games and
closed-source products. This project's license does not require publishing
your game's source or paying royalties.

## Keep a readable credit

Retain the applicable attribution in [NOTICE](../NOTICE), including:

> Includes 3Dgeo Demo / Template by sabino.
> https://github.com/sabino/3dgeo-demo

Apache-2.0 section 4(d) permits a readable attribution in a distributed NOTICE
file, accompanying source/documentation, or the product's normal third-party
notices display. For a game, we recommend the Credits/About screen or a clearly
labelled acknowledgments section in its included manual. A startup splash is
not required. An in-game credit is recommended, not an extra legal condition;
a readable bundled NOTICE is also sufficient under the license.

Include the Apache license, preserve applicable copyright/attribution notices,
and mark changed files as required by section 4. Do not imply that sabino
endorses your product. This guide summarizes the license; it adds no conditions
and the actual license text controls.

## Third-party components are separate

Relicensing this project's own code does not relicense dependencies:

- ngdevkit's runtime and nullsound components are supplied by the SDK under
  LGPL-3.0-or-later. A proprietary game using these components still needs to
  satisfy the applicable LGPL requirements. For statically linked combined
  works, this can include providing material needed to relink against a
  modified library and allowing the required reverse engineering; merely
  displaying a credit is not the whole obligation.
- The toolchain includes components with their own licenses and exceptions.
  Using a compiler alone does not assign its license to all your original code;
  incorporated runtime code must be considered separately.
- Packaged GnGeo definitions also contain upstream interface assets. Their
  copyright notices and applicable license texts are included in `licenses/`
  with the binary bundle. No proprietary BIOS or commercial game ROM is included.

The build records installed package versions in `toolchain.txt` and includes
upstream package copyright notices. Review those components and their source
availability for your particular release; this is not a legal clearance for
every downstream commercial product. Obtain legal advice for a commercial
distribution if you are unsure of its obligations.

## Historical versions

The owner directed the change from GPL-3.0-or-later to Apache-2.0 for the current
project-owned code. Original commits and `legacy-demo-baseline` are unchanged
and still show the terms used at the time. This change does not revoke rights
already granted for those historical versions. Third-party notices are retained.
New contributions are accepted under Apache-2.0 unless otherwise agreed.

References: [Apache terms](https://www.apache.org/licenses/LICENSE-2.0),
[Apache FAQ](https://www.apache.org/foundation/license-faq.html),
[ngdevkit](https://github.com/dciabrin/ngdevkit), and
[LGPL 3.0](https://www.gnu.org/licenses/lgpl-3.0.html).
