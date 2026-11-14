#!/bin/sh
# runs every 15 minutes from launchd (see install_agent.sh), in the see directory
# history.csv: the 5-minute rows of the last 7 days, merged from each new past_24_hours_data.csv

cutoff=$(( $(date +%s) - 7 * 86400 ))
{
    echo timestamp,keypresses,mousemoves,leftclicks,rightclicks,middleclicks
    cat history.csv past_24_hours_data.csv 2>/dev/null | grep '^[0-9]' | sort -t, -k1,1n -u | awk -F, -v c="$cutoff" '$1 > c'
} > history.csv.tmp
# never replace the history with a header-only file
[ "$(wc -l < history.csv.tmp)" -gt 1 ] && mv history.csv.tmp history.csv

# unsigned commits: launchd cannot answer an SSH signing passphrase prompt
git diff --quiet -- *.csv || git -c commit.gpgsign=false commit -qm ud -- *.csv
git push -q
