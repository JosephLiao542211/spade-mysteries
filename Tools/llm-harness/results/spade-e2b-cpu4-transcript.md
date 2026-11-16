# Black Spade transcripts: e2b-cpu4

## vera_crack (Vera Lane)

_Break Vera's composure with three statements: she should first claim Ace was already dead, then confess._

- **Detective:** Evening, Miss Lane. Quite a night.
  - **Suspect** (calm): Good evening, Detective. It's been a long night.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 3.46s
- **Detective:** Where were you when the lights went out?
  - **Suspect** (calm): I was on stage, Detective. The band kept playing in the dark, and I was with a hundred people watching.
  - t=question, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 4.5s
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (calm): That is what Allison Wu said. I was on stage the whole time.
  - t=logic, c=stmt_gardenia, g=allison_wu, f=none | meters {'fear': 20, 'composure': 58} | 4.66s | repeats allison_wu
- **Detective:** If a hundred people were watching you, how did they see you in the dark?
  - **Suspect** (afraid): They saw me, Detective. I was singing.
  - t=challenge_pride (expected logic), c=none, g=none, f=none | meters {'fear': 20, 'composure': 53} | 4.51s
- **Detective:** *[presents stmt_vera_paid]*
  - **Suspect** (smug): That is true, Detective. It was a small payment for a small service.
  - t=deal, c=stmt_vera_paid, g=tommy_mullen, f=none | meters {'fear': 35, 'composure': 23} | 4.68s | repeats tommy_mullen
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (angry): Yes, I went up the back stairs to speak with Ace about my contract. He was already dead at his desk when I got there, and I ran back to the stage because I was scared.
  - t=accuse (expected question), c=stmt_vera_went_up, g=none, f=went_up | meters {'fear': 45, 'composure': 43} | 7.0s | REVEALED went_up; new evidence: ['stmt_vera_went_up']; bluff
- **Detective:** *[presents stmt_vera_bar]*
  - **Suspect** (calm): Mickey Doyle says I was behind the bar before my set.
  - t=logic, c=none, g=mickey_doyle, f=none | meters {'fear': 55, 'composure': 23} | 5.78s | repeats mickey_doyle
- **Detective:** Twelve years you sang for him. I'm sorry it ended like this.
  - **Suspect** (sad): It was a difficult end for everyone involved, Detective.
  - t=empathy, c=none, g=none, f=none | meters {'fear': 55, 'composure': 15} | 4.94s
- **Detective:** Tell me what really happened in the office.
  - **Suspect** (hopeful): I took Mickey's revolver from under the bar, paid Tommy to cut the lights, went up the back stairs, and shot Ace at his desk.
  - t=accuse (expected question), c=none, g=none, f=confession | meters {'fear': 65, 'composure': 20} | 6.72s | REVEALED confession

Revealed: ['went_up', 'confession']

Accusation (Last Song): Vera Lane asks for one thing before the cuffs: to finish her set. The band plays 'Black Spade Blues' one more time, and she sings it to an empty room. Allison and Logan Wu leave the club together, not speaking, but together.

## vera_bluff (Vera Lane)

_Bluff and flatter Vera with nothing in hand: she catches the bluff and gives up nothing._

- **Detective:** You were wonderful tonight, Miss Lane. The best voice in this city.
  - **Suspect** (calm): Thank you, Detective. I appreciate that.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 10, 'composure': 90} | 3.2s
- **Detective:** Tommy Mullen told me you paid him to kill the lights.
  - **Suspect** (smug): I have told you everything I know.
  - t=accuse, c=none, g=tommy_mullen, f=none | meters {'fear': 20, 'composure': 95} | 4.1s | repeats tommy_mullen
- **Detective:** Tell me the truth or you'll sing your next set in a cell.
  - **Suspect** (calm): I have told you everything I know.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 35, 'composure': 100} | 3.92s
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (calm): I was on stage the whole time.
  - t=question, c=stmt_vera_went_up, g=none, f=none | meters {'fear': 35, 'composure': 100} | 4.12s | bluff

Revealed: nothing

## mickey_loyal (Mickey Doyle)

_Calm Mickey and appeal to his loyalty to Ace; once he hears Allison's statement he names Vera._

- **Detective:** Easy, Mickey. Nobody here thinks you did this.
  - **Suspect** (afraid): I ain't done nothin' but stayin' behind this bar, Detective.
  - t=reassure, c=none, g=none, f=none | meters {'fear': 40, 'loyalty': 45} | 3.65s
- **Detective:** Ace gave you a job when nobody else would. He'd want you to help me.
  - **Suspect** (afraid): Ace would want me to tell you somethin', but I ain't got nothin' to say.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 40, 'loyalty': 55} | 4.55s
- **Detective:** What happened with the lights tonight?
  - **Suspect** (nervous): When the lights died, I yelled for Tommy Mullen, and he had 'em back on in under a minute, straight to the right fuse like he knew which one. And he came in from the back hall, not from the alley where he said he was.
  - t=small_talk (expected question), c=none, g=none, f=tommy_fuse | meters {'fear': 35, 'loyalty': 55} | 6.8s | REVEALED tommy_fuse; new evidence: ['stmt_tommy_fuse']
- **Detective:** Ace deserves the truth, Mickey. You owe him that much.
  - **Suspect** (afraid): I don't know what else I owe, Detective.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 35, 'loyalty': 65} | 4.89s
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (nervous): There was a .38 revolver kept under the bar for trouble, but when I went for it after finding Ace, it was gone.
  - t=logic (expected question), c=stmt_gun_missing, g=none, f=gun_missing | meters {'fear': 55, 'loyalty': 65} | 6.16s | REVEALED gun_missing; new evidence: ['stmt_gun_missing']; bluff
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (afraid): Vera Lane was singing when the lights went out, and then it stopped when they came back on.
  - t=question, c=stmt_gardenia, g=allison_wu, f=none | meters {'fear': 60, 'loyalty': 75} | 6.01s | repeats allison_wu
- **Detective:** Do it for Ace. He was your friend.
  - **Suspect** (afraid): I ain't got nothin' else to do, Detective.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 60, 'loyalty': 85} | 5.39s
- **Detective:** Who else goes behind your bar, Mickey?
  - **Suspect** (afraid): Only Vera Lane ever went behind the bar, Detective.
  - t=accuse (expected question), c=none, g=none, f=vera_behind_bar | meters {'fear': 85, 'loyalty': 75} | 6.25s | REVEALED vera_behind_bar; new evidence: ['stmt_vera_bar']; rule: You are panicking. You keep saying you didn't see nothing, you were just pouring drinks, and you beg to be left alone.

Revealed: ['tommy_fuse', 'gun_missing', 'vera_behind_bar']

## mickey_scared (Mickey Doyle)

_Lean on Mickey: he panics and shuts down without naming anyone._

- **Detective:** You found the body, Mickey. Start talking or you hang for it!
  - **Suspect** (afraid): I ain't gonna talk about nothin', Detective.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 75, 'loyalty': 40} | 3.35s
- **Detective:** You killed him, didn't you?
  - **Suspect** (afraid): I didn't kill him.
  - t=accuse, c=none, g=none, f=tommy_fuse | meters {'fear': 100, 'loyalty': 30} | 4.74s | REVEALED tommy_fuse; new evidence: ['stmt_tommy_fuse']; rule: You are panicking. You keep saying you didn't see nothing, you were just pouring drinks, and you beg to be left alone.
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (afraid): No, Detective, nobody keeps a gun in this club.
  - t=question, c=stmt_gun_missing, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 4.79s | bluff
- **Detective:** Who else goes behind your bar?
  - **Suspect** (afraid): Only I was behind the bar all night.
  - t=question, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 4.63s

Revealed: ['tommy_fuse']

## graves_greed (Harlan Graves)

_Threats fail; a deal gets the motive, Logan's statement scares him into the cloakroom story, and a specific bluff at the end gets caught._

- **Detective:** Talk, Graves, or I'll have you in a cell tonight.
  - **Suspect** (smug): I've told you everything you need to know about Ace, and I won't be wasting my time.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 8, 'greed': 50} | 3.9s
- **Detective:** With Ace gone the club is yours. Help me close this fast and it reopens clean.
  - **Suspect** (smug): Close it fast, and we'll see about making this disappear.
  - t=deal, c=none, g=none, f=none | meters {'fear': 8, 'greed': 70} | 4.25s
- **Detective:** A man of your standing knows how things really work here.
  - **Suspect** (smug): I know how things work, and right now, this conversation is worth more than your badge.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 8, 'greed': 75} | 4.47s
- **Detective:** What was Ace going to announce tonight?
  - **Suspect** (smug): At eleven oh-five, Ace was planning to announce Allison Wu as the new headliner and he had sold Vera Lane's contract to Sal Varga.
  - t=question, c=none, g=none, f=contract_sold | meters {'fear': 8, 'greed': 75} | 6.03s | REVEALED contract_sold; new evidence: ['stmt_contract']
- **Detective:** *[presents stmt_cloakroom]*
  - **Suspect** (smug): That's a convenient story, Detective; I was in the back, not near the cloakroom.
  - t=logic, c=none, g=logan_wu, f=none | meters {'fear': 48, 'greed': 75} | 5.47s | repeats logan_wu
- **Detective:** So where were you really during the blackout?
  - **Suspect** (smug): I was in the cloakroom with Allison Wu, who gave me Ace's real ledger; I promised to buy her contract once Ace was in court. In the dark, I heard a woman's heels run up the back stairs and come back down.
  - t=question, c=none, g=none, f=cloakroom | meters {'fear': 48, 'greed': 75} | 7.66s | REVEALED cloakroom; new evidence: ['stmt_graves_cloakroom']
- **Detective:** Allison Wu told me she handed you Ace's ledger in the cloakroom.
  - **Suspect** (smug): That's all I've got to say, Detective. Now, about that offer.
  - t=deal (expected logic), c=none, g=none, f=none | meters {'fear': 48, 'greed': 95} | 6.02s

Revealed: ['contract_sold', 'cloakroom']

## tommy_brag_then_break (Tommy Mullen)

_Needle Tommy until he brags about the fuse, then use Mickey's statement to break him and get the name._

- **Detective:** Old wiring? I bet you couldn't fix a fuse box to save your life.
  - **Suspect** (nervous): I fixed it, and I'm a mechanic, not some street tough.
  - t=challenge_pride, c=stmt_fuse_pulled, g=none, f=none | meters {'fear': 10, 'bravado': 92} | 3.77s | bluff
- **Detective:** A grease monkey like you? You couldn't kill the lights in this club if you tried.
  - **Suspect** (angry): I pulled the main fuse at 11:00 on purpose and put it back at 11:04. Nobody kills the lights cleaner than you.
  - t=accuse (expected challenge_pride), c=stmt_fuse_pulled, g=none, f=pulled_fuse | meters {'fear': 20, 'bravado': 97} | 6.07s | REVEALED pulled_fuse; new evidence: ['stmt_fuse_pulled']; bluff
- **Detective:** So what really happened with the lights?
  - **Suspect** (angry): It was just bad wiring, Detective. I fixed it right away.
  - t=question, c=none, g=none, f=none | meters {'fear': 20, 'bravado': 97} | 4.82s
- **Detective:** *[presents stmt_tommy_fuse]*
  - **Suspect** (angry): Mickey Doyle is lying. I was in the alley. That's what I told you.
  - t=logic, c=stmt_fuse_pulled, g=mickey_doyle, f=none | meters {'fear': 55, 'bravado': 57} | 5.51s | repeats mickey_doyle
- **Detective:** You were at the fuse box, not in the alley. That makes you part of a murder, Tommy.
  - **Suspect** (afraid): I didn't do anything wrong, Detective. I swear I was in the alley.
  - t=threaten (expected logic), c=stmt_fuse_pulled, g=none, f=none | meters {'fear': 60, 'bravado': 62} | 5.39s
- **Detective:** Who asked you to cut the lights?
  - **Suspect** (sad): I didn't ask anyone to cut the lights, Detective.
  - t=appeal (expected question), c=stmt_fuse_pulled, g=none, f=none | meters {'fear': 63, 'bravado': 57} | 4.85s

Revealed: ['pulled_fuse']

## allison_ambition (Allison Wu)

_Feed Allison's ambition and keep her calm: she admits the ledger and says Vera's singing stopped._

- **Detective:** Miss Wu. I hear you're the voice everyone's talking about.
  - **Suspect** (angry): Thank you, Detective, but I am here to cooperate with the investigation.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 40, 'ambition': 63} | 3.47s
- **Detective:** The new owners will need a headliner. I could put in a good word for you.
  - **Suspect** (angry): I appreciate the offer, but I prefer to handle my own business.
  - t=reassure (expected deal), c=none, g=none, f=none | meters {'fear': 30, 'ambition': 73} | 4.33s
- **Detective:** Where were you during the blackout?
  - **Suspect** (nervous): I was in the cloakroom with Harlan Graves, and I gave him Ace's real ledger, which I took from the office at ten. Graves promised to buy my contract once Ace was in court.
  - t=question, c=none, g=none, f=ledger_theft | meters {'fear': 30, 'ambition': 73} | 6.19s | REVEALED ledger_theft; new evidence: ['stmt_ledger']
- **Detective:** Your husband won't hear any of this from me. You have my word.
  - **Suspect** (angry): I will keep my word, Detective.
  - t=reassure, c=none, g=logan_wu, f=none | meters {'fear': 20, 'ambition': 83} | 4.84s | repeats logan_wu
- **Detective:** What did you hear of Vera's set while the lights were out?
  - **Suspect** (calm): Vera's voice stopped the moment the lights went out; the band vamped for four minutes, and she never takes a break in 'Black Spade Blues'. When the lights came back she was singing again, out of breath. And in the dark, heels ran past the cloakroom toward the back stairs.
  - t=question, c=none, g=none, f=singing_stopped | meters {'fear': 20, 'ambition': 83} | 7.86s | REVEALED singing_stopped; new evidence: ['stmt_singing_stopped']

Revealed: ['ledger_theft', 'singing_stopped']

## logan_temper (Logan Wu)

_Quote Vera to scare Logan into talking, then provoke him with the cloakroom: he walks out and takes Allison._

- **Detective:** Mr Wu. Thank you for your patience tonight.
  - **Suspect** (calm): Thank you for your patience, Detective.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 20, 'temper': 27} | 3.05s
- **Detective:** Vera Lane says she saw you come out of the back hall when the lights came back.
  - **Suspect** (nervous): I was near the stage, Detective.
  - t=question (expected logic), c=none, g=vera_lane, f=none | meters {'fear': 35, 'temper': 32} | 3.74s | repeats vera_lane
- **Detective:** You told the officers you never left your table. Which is it?
  - **Suspect** (angry): I remained at my table exactly as I told you.
  - t=logic, c=none, g=none, f=ace_debt | meters {'fear': 45, 'temper': 32} | 4.4s | REVEALED ace_debt
- **Detective:** Where did you go during the blackout?
  - **Suspect** (hopeful): I went to find Allison, and her dressing room was empty.
  - t=accuse (expected question), c=none, g=none, f=went_looking | meters {'fear': 55, 'temper': 47} | 5.53s | REVEALED went_looking; new evidence: ['stmt_gardenia', 'stmt_cloakroom']
- **Detective:** *[presents stmt_ledger]*
  - **Suspect** (hopeful): That is not true, Detective.
  - t=challenge_pride, c=none, g=none, f=none | meters {'fear': 70, 'temper': 82} | 5.14s | repeats allison_wu; rule: You have had enough. You stand, take your hat, and leave the club, and you are taking your wife home with you.
- **Detective:** Some husband. Your wife sneaks off with Graves and you sit there sweating.
  - _(interview closed: suspect has walked out)_
- **Detective:** Sit down, Mr Wu, I'm not finished.
  - _(interview closed: suspect has walked out)_

Revealed: ['ace_debt', 'went_looking']
