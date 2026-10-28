#!/bin/bash
# Runs the Mac build gate FROM the Windows PC (D-003): pushes HEAD to the Mac and builds it there over SSH.
# Needs the `dt-mac` host alias in ~/.ssh/config and the `mac` git remote (see Docs/STATE.md).
# Exit 0 = passed, 2 = NOT RUN (Mac toolchain missing or Mac unreachable), anything else = failed.
set -uo pipefail
cd "$(dirname "$0")/.."
SHA="$(git rev-parse HEAD)"

if ! ssh -o BatchMode=yes -o ConnectTimeout=8 dt-mac true 2>/dev/null; then
	echo "NOT RUN: Mac unreachable (dt-mac)"
	exit 2
fi

git push -q -f mac "HEAD:refs/heads/gate" || exit 1
ssh -o BatchMode=yes dt-mac "export PATH=/opt/homebrew/bin:\$PATH; cd ~/Dev/Dusktracer && git fetch -q origin gate && git checkout -q --detach $SHA && bash Tools/mac_build.sh"
RC=$?
[ "$RC" = 0 ] && echo "Mac gate PASSED at $SHA" || echo "Mac gate exit $RC at $SHA"
exit $RC
