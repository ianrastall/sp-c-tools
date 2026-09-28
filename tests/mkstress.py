"""Build tests/data/stress.pgn from parity.pgn, exercising the paths parity
lacks: duplicate games (same and different players), error terminations,
and FEN-start games (made by a stock pgn-extract's --dropply).

    python tests/mkstress.py tests/data/parity.pgn <stock pgn-extract.exe> \\
        tests/data/stress.pgn [tests/data/comments.pgn]

With a fourth argument it also writes a commented variant of the same games
for SGS, which keeps comments: [%clk]/[%eval] comments, long ones that wrap
onto lines starting with '[', NAGs, one-move variations, and a few comments
containing '[White "' (which the batches' `find /C` counts as games).

Seeded, so the same parity.pgn and pgn-extract (v24-11 was used for the
goldens) reproduce the same files; tests/check.sh checks their hashes."""
import os, random, re, subprocess, sys

src, pe, out = sys.argv[1], sys.argv[2], sys.argv[3]
out_comments = sys.argv[4] if len(sys.argv) > 4 else None
text = open(src, encoding='latin-1').read().replace('\r\n', '\n')
games = [g.strip() + '\n' for g in re.split(r'\n\n(?=\[Event )', text) if g.strip()]
rng = random.Random(7)
decisive = [g for g in games if '[Result "1-0"]' in g or '[Result "0-1"]' in g]

def set_tag(g, tag, val):
    if re.search(r'^\[%s "' % tag, g, re.M):
        return re.sub(r'^\[%s "[^"]*"\]' % tag, '[%s "%s"]' % (tag, val), g, flags=re.M)
    return g.replace('\n\n', '\n[%s "%s"]\n\n' % (tag, val), 1)

extra = []
# exact duplicates of decisive games (dedup must drop them)
extra += rng.sample(decisive, 80)
# same moves, different players (dup key ignores tags)
players = sorted(set(re.findall(r'^\[White "([^"]*)"', text, re.M)))
for g in rng.sample(decisive, 40):
    extra.append(set_tag(g, 'White', rng.choice(players)))
# error terminations, some of which also duplicate a clean game
for g in rng.sample(decisive, 60):
    extra.append(set_tag(g, 'Termination',
                         rng.choice(['time forfeit', 'abandoned', 'rules infraction',
                                     'illegal move', 'normal'])))
# FEN starts
fen_src = '\n\n'.join(rng.sample(decisive, 120)) + '\n'
open(out + '.fensrc', 'w', encoding='latin-1', newline='\n').write(fen_src)
for drop in (9, 16, 30):
    r = subprocess.run([pe, '--quiet', '--dropply', str(drop), out + '.fensrc'],
                       capture_output=True, text=True, encoding='latin-1')
    fg = [g.strip() + '\n' for g in re.split(r'\n\n(?=\[Event )', r.stdout) if g.strip()]
    extra += fg[:40]
os.remove(out + '.fensrc')

allg = games + extra
rng.shuffle(allg)
open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(allg))
print(len(games), 'base +', len(extra), 'extra; FEN games:',
      sum('[FEN ' in g for g in allg))

if out_comments:
    crng = random.Random(11)   # separate stream: stress.pgn is unchanged
    RESULTS = {'1-0', '0-1', '1/2-1/2', '*'}

    def annotate(game):
        head, sep, moves = game.partition('\n\n')
        toks = moves.split()
        outt = []
        num, black = 1, False
        for t in toks:
            m = re.match(r'^(\d+)(\.+)$', t)
            if m:
                num, black = int(m.group(1)), len(m.group(2)) > 1
                outt.append(t)
                continue
            if t in RESULTS:
                outt.append(t)
                continue
            outt.append(t)
            r = crng.random()
            if r < 0.04:   # the move again, as a variation of itself
                outt.append('(%d%s %s)' % (num, '...' if black else '.', t))
            elif r < 0.07:
                outt.append(crng.choice(['$1', '$2', '$6', '$14']))
            if crng.random() < 0.6:
                clk = '[%%clk 0:%02d:%02d]' % (crng.randrange(60), crng.randrange(60))
                if crng.random() < 0.15:
                    outt.append('{[%%eval %+.2f] %s long engine remark to force the '
                                'comment across a line break [%%depth %d]}'
                                % (crng.uniform(-3, 3), clk, crng.randrange(10, 40)))
                else:
                    outt.append('{%s}' % clk)
            if black:
                num += 1
            black = not black
        if crng.random() < 0.02:
            outt.insert(0, '{Note: [White "X"] would be a tag}')
        return head + sep + ' '.join(outt) + '\n'

    cg = [annotate(g) if crng.random() < 0.7 else g for g in allg]
    open(out_comments, 'w', encoding='latin-1', newline='\n').write('\n'.join(cg))
    print('commented variant:', sum('{' in g for g in cg), 'games with comments')
