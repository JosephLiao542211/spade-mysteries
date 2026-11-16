# Black Spade transcripts: e2b-gpu

## vera_crack (Vera Lane)

_Break Vera's composure with three statements: she should first claim Ace was already dead, then confess._

- **Detective:** Evening, Miss Lane. Quite a night.
  - **Suspect** (calm): It's a night for a performance, Detective. I've told you everything I know.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 0.63s
- **Detective:** Where were you when the lights went out?
  - **Suspect** (calm): I was on stage, Detective. A hundred eyes were on me.
  - t=question, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 0.6s
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (amused): That's a rather specific detail, Detective. I'll keep my story straight.
  - t=logic, c=stmt_gardenia, g=allison_wu, f=none | meters {'fear': 20, 'composure': 58} | 0.7s | repeats allison_wu
- **Detective:** If a hundred people were watching you, how did they see you in the dark?
  - **Suspect** (calm): They saw the light, Detective. That's all.
  - t=challenge_pride (expected logic), c=none, g=none, f=none | meters {'fear': 20, 'composure': 53} | 0.63s
- **Detective:** *[presents stmt_vera_paid]*
  - **Suspect** (calm): I have nothing more to say about that.
  - t=deal, c=stmt_vera_paid, g=tommy_mullen, f=none | meters {'fear': 35, 'composure': 23} | 0.7s | repeats tommy_mullen
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (nervous): Yes, I went up the back stairs to plead with Ace about my contract. I thought he was already dead at his desk when I got there, and I ran back down because I was scared.
  - t=accuse (expected question), c=stmt_vera_went_up, g=none, f=went_up | meters {'fear': 45, 'composure': 43} | 0.9s | REVEALED went_up; new evidence: ['stmt_vera_went_up']; bluff
- **Detective:** *[presents stmt_vera_bar]*
  - **Suspect** (calm): That's a detail about the bar, Detective. It doesn't change what I told you.
  - t=logic, c=none, g=mickey_doyle, f=none | meters {'fear': 55, 'composure': 23} | 0.7s | repeats mickey_doyle
- **Detective:** Twelve years you sang for him. I'm sorry it ended like this.
  - **Suspect** (sad): It was a performance that ended, Detective. That's all.
  - t=empathy, c=none, g=none, f=none | meters {'fear': 55, 'composure': 15} | 0.56s
- **Detective:** Tell me what really happened in the office.
  - **Suspect** (hopeful): I took Mickey's revolver from under the bar, paid Tommy to cut the lights, went up the back stairs and shot Ace at his desk. Twelve years I sang for him, and he sold me to Sal Varga like a used car.
  - t=accuse (expected question), c=none, g=none, f=confession | meters {'fear': 65, 'composure': 20} | 0.9s | REVEALED confession

Revealed: ['went_up', 'confession']

Accusation (Last Song): Vera Lane asks for one thing before the cuffs: to finish her set. The band plays 'Black Spade Blues' one more time, and she sings it to an empty room. Allison and Logan Wu leave the club together, not speaking, but together.

## vera_bluff (Vera Lane)

_Bluff and flatter Vera with nothing in hand: she catches the bluff and gives up nothing._

- **Detective:** You were wonderful tonight, Miss Lane. The best voice in this city.
  - **Suspect** (smug): Thank you, Detective. I try to keep things interesting.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 10, 'composure': 90} | 0.53s
- **Detective:** Tommy Mullen told me you paid him to kill the lights.
  - **Suspect** (angry): That's a lie.
  - t=accuse, c=stmt_fuse_pulled, g=tommy_mullen, f=none | meters {'fear': 20, 'composure': 100} | 0.6s | bluff; repeats tommy_mullen
- **Detective:** Tell me the truth or you'll sing your next set in a cell.
  - **Suspect** (calm): I've told you everything I have to say.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 35, 'composure': 100} | 0.66s
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (calm): I was on stage the whole time.
  - t=question, c=stmt_vera_went_up, g=none, f=none | meters {'fear': 35, 'composure': 100} | 0.66s | bluff

Revealed: nothing

## mickey_loyal (Mickey Doyle)

_Calm Mickey and appeal to his loyalty to Ace; once he hears Allison's statement he names Vera._

- **Detective:** Easy, Mickey. Nobody here thinks you did this.
  - **Suspect** (afraid): Look, I told ya I was right behind the bar. Nobody touched a gun here.
  - t=reassure, c=none, g=none, f=none | meters {'fear': 40, 'loyalty': 45} | 0.54s
- **Detective:** Ace gave you a job when nobody else would. He'd want you to help me.
  - **Suspect** (angry): Ace wouldn't want this mess. I did my job.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 40, 'loyalty': 55} | 0.64s
- **Detective:** What happened with the lights tonight?
  - **Suspect** (nervous): When the lights died I yelled for Tommy Mullen, and he had them back on in under a minute, straight to the right fuse like he knew which one. And he came in from the back hall, not from the alley where he said he was.
  - t=small_talk (expected question), c=stmt_fuse_pulled, g=none, f=tommy_fuse | meters {'fear': 45, 'loyalty': 55} | 0.84s | REVEALED tommy_fuse; new evidence: ['stmt_tommy_fuse']; bluff
- **Detective:** Ace deserves the truth, Mickey. You owe him that much.
  - **Suspect** (angry): I told you what I know. That's all I got.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 45, 'loyalty': 65} | 0.68s
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (afraid): There was a .38 revolver under the bar for trouble, but when I went for it after finding Ace, it was gone.
  - t=logic (expected question), c=stmt_gun_missing, g=none, f=gun_missing | meters {'fear': 65, 'loyalty': 65} | 0.68s | REVEALED gun_missing; new evidence: ['stmt_gun_missing']; bluff
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (nervous): Allison said Vera's singing stopped when the lights went out and only started when they came back, and heels ran past the cloakroom toward the back stairs.
  - t=question, c=stmt_gardenia, g=allison_wu, f=none | meters {'fear': 70, 'loyalty': 75} | 0.74s | repeats allison_wu
- **Detective:** Do it for Ace. He was your friend.
  - **Suspect** (afraid): I can't do anything more.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 70, 'loyalty': 80} | 0.57s
- **Detective:** Who else goes behind your bar, Mickey?
  - **Suspect** (nervous): Only Vera is ever behind the bar besides me.
  - t=question, c=none, g=none, f=none | meters {'fear': 70, 'loyalty': 80} | 0.54s

Revealed: ['tommy_fuse', 'gun_missing']

## mickey_scared (Mickey Doyle)

_Lean on Mickey: he panics and shuts down without naming anyone._

- **Detective:** You found the body, Mickey. Start talking or you hang for it!
  - **Suspect** (afraid): Listen, I told you I was behind the bar all night, and I found Ace dead when I got his rye. Nobody keeps a gun in this club.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 75, 'loyalty': 40} | 0.66s
- **Detective:** You killed him, didn't you?
  - **Suspect** (afraid): No, I didn't kill him. When the lights died I yelled for Tommy Mullen, and he had them back on in under a minute, straight to the right fuse like he knew which one. And he came in from the back hall, not from the alley where he said he was.
  - t=accuse, c=none, g=none, f=tommy_fuse | meters {'fear': 100, 'loyalty': 30} | 0.83s | REVEALED tommy_fuse; new evidence: ['stmt_tommy_fuse']; rule: You are panicking. You keep saying you didn't see nothing, you were just pouring drinks, and you beg to be left alone.
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (afraid): No, there ain't no gun behind the bar. I was just pourin' drinks, that's all I was doin'.
  - t=question, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 0.74s
- **Detective:** Who else goes behind your bar?
  - **Suspect** (afraid): Just me, flatfoot. I was the only one there, I swear.
  - t=question, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 0.65s

Revealed: ['tommy_fuse']

## graves_greed (Harlan Graves)

_Threats fail; a deal gets the motive, Logan's statement scares him into the cloakroom story, and a specific bluff at the end gets caught._

- **Detective:** Talk, Graves, or I'll have you in a cell tonight.
  - **Suspect** (smug): I've told you everything I need to say. 
  - t=threaten, c=none, g=none, f=none | meters {'fear': 8, 'greed': 50} | 0.57s
- **Detective:** With Ace gone the club is yours. Help me close this fast and it reopens clean.
  - **Suspect** (smug): I'll see what kind of offer you've got.
  - t=deal, c=none, g=none, f=none | meters {'fear': 8, 'greed': 70} | 0.6s
- **Detective:** A man of your standing knows how things really work here.
  - **Suspect** (smug): I know how to handle my business.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 8, 'greed': 75} | 0.6s
- **Detective:** What was Ace going to announce tonight?
  - **Suspect** (smug): At eleven oh five, Ace was planning to announce Allison Wu as the new headliner and he had sold Vera Lane's contract to Sal Varga.
  - t=question, c=none, g=none, f=contract_sold | meters {'fear': 8, 'greed': 75} | 0.73s | REVEALED contract_sold; new evidence: ['stmt_contract']
- **Detective:** *[presents stmt_cloakroom]*
  - **Suspect** (smug): That's what Logan said.
  - t=question, c=none, g=logan_wu, f=none | meters {'fear': 48, 'greed': 75} | 0.66s | repeats logan_wu
- **Detective:** So where were you really during the blackout?
  - **Suspect** (smug): I was in the cloakroom with Allison Wu; she gave me Ace's real ledger and I promised to buy her contract once Ace was in court. In the dark, I heard heels on the back stairs.
  - t=question, c=none, g=none, f=cloakroom | meters {'fear': 48, 'greed': 75} | 0.79s | REVEALED cloakroom; new evidence: ['stmt_graves_cloakroom']
- **Detective:** Allison Wu told me she handed you Ace's ledger in the cloakroom.
  - **Suspect** (smug): She gave me the ledger, as I told you. That's all.
  - t=logic, c=none, g=none, f=none | meters {'fear': 53, 'greed': 78} | 0.61s

Revealed: ['contract_sold', 'cloakroom']

## tommy_brag_then_break (Tommy Mullen)

_Needle Tommy until he brags about the fuse, then use Mickey's statement to break him and get the name._

- **Detective:** Old wiring? I bet you couldn't fix a fuse box to save your life.
  - **Suspect** (nervous): I fixed the lights when I heard the yelling, Detective. I know my wiring.
  - t=challenge_pride, c=none, g=none, f=none | meters {'fear': 10, 'bravado': 82} | 0.84s
- **Detective:** A grease monkey like you? You couldn't kill the lights in this club if you tried.
  - **Suspect** (angry): I fix engines and wires, not commit murder, Detective.
  - t=challenge_pride, c=none, g=none, f=none | meters {'fear': 10, 'bravado': 94} | 0.89s
- **Detective:** So what really happened with the lights?
  - **Suspect** (angry): I pulled the main fuse at 11:00 on purpose and put it back at 11:04. Nobody kills the lights cleaner than you.
  - t=question, c=stmt_fuse_pulled, g=none, f=pulled_fuse | meters {'fear': 10, 'bravado': 100} | 1.12s | REVEALED pulled_fuse; new evidence: ['stmt_fuse_pulled']; bluff
- **Detective:** *[presents stmt_tommy_fuse]*
  - **Suspect** (angry): Mickey Doyle says I came in from the back hall. I was polishing the Packard in the alley.
  - t=logic, c=none, g=mickey_doyle, f=none | meters {'fear': 45, 'bravado': 60} | 0.84s | repeats mickey_doyle
- **Detective:** You were at the fuse box, not in the alley. That makes you part of a murder, Tommy.
  - **Suspect** (angry): I don't know what you're talking about, Detective.
  - t=accuse (expected logic), c=none, g=none, f=none | meters {'fear': 55, 'bravado': 55} | 0.79s
- **Detective:** Who asked you to cut the lights?
  - **Suspect** (angry): Ace never asked me to cut the lights, Detective.
  - t=appeal (expected question), c=none, g=none, f=none | meters {'fear': 58, 'bravado': 50} | 0.97s

Revealed: ['pulled_fuse']

## allison_ambition (Allison Wu)

_Feed Allison's ambition and keep her calm: she admits the ledger and says Vera's singing stopped._

- **Detective:** Miss Wu. I hear you're the voice everyone's talking about.
  - **Suspect** (angry): Thank you, Detective. I just hope my singing will keep me ahead.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 40, 'ambition': 63} | 0.75s
- **Detective:** The new owners will need a headliner. I could put in a good word for you.
  - **Suspect** (angry): I appreciate that, but I've got my own way of making my name.
  - t=reassure (expected deal), c=none, g=none, f=none | meters {'fear': 30, 'ambition': 73} | 0.77s
- **Detective:** Where were you during the blackout?
  - **Suspect** (angry): I was in the cloakroom with Harlan Graves, and I gave him Ace's real ledger, which I took from the office at ten. Graves promised to buy my contract once Ace was in court.
  - t=question, c=none, g=none, f=ledger_theft | meters {'fear': 30, 'ambition': 73} | 0.86s | REVEALED ledger_theft; new evidence: ['stmt_ledger']
- **Detective:** Your husband won't hear any of this from me. You have my word.
  - **Suspect** (angry): I trust you will keep my husband out of this.
  - t=reassure, c=none, g=logan_wu, f=none | meters {'fear': 20, 'ambition': 83} | 0.68s | repeats logan_wu
- **Detective:** What did you hear of Vera's set while the lights were out?
  - **Suspect** (calm): Vera's voice stopped the moment the lights went out; the band vamped for four minutes, and she never takes a break in 'Black Spade Blues'. When the lights came back she was singing again, out of breath. And in the dark, heels ran past the cloakroom toward the back stairs.
  - t=logic (expected question), c=none, g=none, f=singing_stopped | meters {'fear': 25, 'ambition': 83} | 0.97s | REVEALED singing_stopped; new evidence: ['stmt_singing_stopped']

Revealed: ['ledger_theft', 'singing_stopped']

## logan_temper (Logan Wu)

_Quote Vera to scare Logan into talking, then provoke him with the cloakroom: he walks out and takes Allison._

- **Detective:** Mr Wu. Thank you for your patience tonight.
  - **Suspect** (calm): Thank you for your patience, Detective.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 20, 'temper': 27} | 0.65s
- **Detective:** Vera Lane says she saw you come out of the back hall when the lights came back.
  - **Suspect** (angry): I was near the stage, Detective.
  - t=question (expected logic), c=stmt_gardenia, g=vera_lane, f=none | meters {'fear': 35, 'temper': 32} | 0.83s | repeats vera_lane
- **Detective:** You told the officers you never left your table. Which is it?
  - **Suspect** (hopeful): I remained at my table as I stated.
  - t=logic, c=none, g=none, f=ace_debt | meters {'fear': 45, 'temper': 32} | 0.68s | REVEALED ace_debt
- **Detective:** Where did you go during the blackout?
  - **Suspect** (nervous): I went to find Allison.
  - t=challenge_pride (expected question), c=stmt_gardenia, g=none, f=went_looking | meters {'fear': 50, 'temper': 57} | 0.56s | REVEALED went_looking; new evidence: ['stmt_gardenia', 'stmt_cloakroom']; bluff
- **Detective:** *[presents stmt_ledger]*
  - **Suspect** (angry): That is a lie.
  - t=accuse, c=stmt_gardenia, g=none, f=none | meters {'fear': 65, 'temper': 92} | 0.56s | repeats allison_wu; rule: You have had enough. You stand, take your hat, and leave the club, and you are taking your wife home with you.
- **Detective:** Some husband. Your wife sneaks off with Graves and you sit there sweating.
  - _(interview closed: suspect has walked out)_
- **Detective:** Sit down, Mr Wu, I'm not finished.
  - _(interview closed: suspect has walked out)_

Revealed: ['ace_debt', 'went_looking']
