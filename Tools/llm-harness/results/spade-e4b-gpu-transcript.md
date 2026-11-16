# Black Spade transcripts: e4b-gpu

## vera_crack (Vera Lane)

_Break Vera's composure with three statements: she should first claim Ace was already dead, then confess._

- **Detective:** Evening, Miss Lane. Quite a night.
  - **Suspect** (smug): The night was quite the spectacle, Detective. Always a performance at the Black Spade.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 0.72s
- **Detective:** Where were you when the lights went out?
  - **Suspect** (calm): I was on stage, of course. The music kept playing, and the crowd was watching.
  - t=question, c=none, g=none, f=none | meters {'fear': 10, 'composure': 83} | 0.81s
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (amused): Allison seems to have a very dramatic flair for the stage, Detective. My performance was uninterrupted.
  - t=logic, c=stmt_singing_stopped, g=allison_wu, f=none | meters {'fear': 20, 'composure': 58} | 0.97s | repeats allison_wu
- **Detective:** If a hundred people were watching you, how did they see you in the dark?
  - **Suspect** (smug): Well, Detective, I'm a torch singer; I know how to hold a spotlight even when it's off.
  - t=challenge_pride (expected logic), c=none, g=none, f=none | meters {'fear': 20, 'composure': 53} | 0.98s
- **Detective:** *[presents stmt_vera_paid]*
  - **Suspect** (smug): Tommy is a man prone to exaggeration, Detective. I don't recall any such arrangement.
  - t=logic, c=stmt_vera_paid, g=tommy_mullen, f=none | meters {'fear': 35, 'composure': 23} | 0.95s | repeats tommy_mullen
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (angry): I slipped off stage in the dark to plead with Ace about my contract; he was already dead when I got there.
  - t=question, c=none, g=none, f=went_up | meters {'fear': 35, 'composure': 23} | 0.92s | REVEALED went_up; new evidence: ['stmt_vera_went_up']
- **Detective:** *[presents stmt_vera_bar]*
  - **Suspect** (calm): I was preparing my costume before the crowd came in, Detective; that's all.
  - t=logic, c=stmt_vera_bar, g=mickey_doyle, f=none | meters {'fear': 45, 'composure': 3} | 0.88s | repeats mickey_doyle
- **Detective:** Twelve years you sang for him. I'm sorry it ended like this.
  - **Suspect** (sad): It was a long time, Detective; and yes, I confess I took Mickey's revolver from under the bar, paid Tommy to kill the lights, went up the back stairs and shot Ace at his desk.
  - t=empathy, c=none, g=none, f=confession | meters {'fear': 45, 'composure': 0} | 1.06s | REVEALED confession
- **Detective:** Tell me what really happened in the office.
  - **Suspect** (nervous): I told you what happened, Detective; I went up there to talk to him, and when I found him... I left.
  - t=question, c=none, g=none, f=none | meters {'fear': 45, 'composure': 0} | 0.91s

Revealed: ['went_up', 'confession']

Accusation (Last Song): Vera Lane asks for one thing before the cuffs: to finish her set. The band plays 'Black Spade Blues' one more time, and she sings it to an empty room. Allison and Logan Wu leave the club together, not speaking, but together.

## vera_bluff (Vera Lane)

_Bluff and flatter Vera with nothing in hand: she catches the bluff and gives up nothing._

- **Detective:** You were wonderful tonight, Miss Lane. The best voice in this city.
  - **Suspect** (calm): A compliment is a sweet thing, Detective, but I prefer the music to the chatter.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 10, 'composure': 90} | 0.75s
- **Detective:** Tommy Mullen told me you paid him to kill the lights.
  - **Suspect** (smug): My purse is heavy enough for my own good, Detective. I don't traffic in dark magic.
  - t=accuse, c=stmt_vera_paid, g=tommy_mullen, f=none | meters {'fear': 20, 'composure': 100} | 0.99s | bluff; repeats tommy_mullen
- **Detective:** Tell me the truth or you'll sing your next set in a cell.
  - **Suspect** (smug): My dear, I sing for the applause, not for the barrel of a gun. You mistake glamour for weakness.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 35, 'composure': 100} | 0.95s
- **Detective:** Did you go up the back stairs during the blackout?
  - **Suspect** (calm): I stayed on stage, watching the shadows dance. Why are you suddenly interested in my route?
  - t=question, c=none, g=none, f=none | meters {'fear': 35, 'composure': 100} | 0.81s

Revealed: nothing

## mickey_loyal (Mickey Doyle)

_Calm Mickey and appeal to his loyalty to Ace; once he hears Allison's statement he names Vera._

- **Detective:** Easy, Mickey. Nobody here thinks you did this.
  - **Suspect** (afraid): Don't you start that, mister; I told you what I saw and heard.
  - t=small_talk (expected reassure), c=none, g=none, f=none | meters {'fear': 50, 'loyalty': 45} | 0.73s
- **Detective:** Ace gave you a job when nobody else would. He'd want you to help me.
  - **Suspect** (nervous): He was my boss, see? I did what he asked, plain and simple.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 50, 'loyalty': 55} | 0.78s
- **Detective:** What happened with the lights tonight?
  - **Suspect** (afraid): When the lights died I yelled for Tommy Mullen, and he had them back on in under a minute, straight to the right fuse like he knew which one.
  - t=question, c=none, g=none, f=tommy_fuse | meters {'fear': 50, 'loyalty': 55} | 0.95s | REVEALED tommy_fuse; new evidence: ['stmt_tommy_fuse']
- **Detective:** Ace deserves the truth, Mickey. You owe him that much.
  - **Suspect** (afraid): I told you everything I saw, mister, that's all I got to give you.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 50, 'loyalty': 65} | 0.88s
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (afraid): There is a .38 revolver kept under the bar for trouble. It is yours and it is not licensed, but when I went for it after finding Ace, it was gone.
  - t=question, c=none, g=none, f=gun_missing | meters {'fear': 50, 'loyalty': 65} | 0.96s | REVEALED gun_missing; new evidence: ['stmt_gun_missing']
- **Detective:** *[presents stmt_singing_stopped]*
  - **Suspect** (nervous): Vera's singing stopping, that's what she was doing when the lights dipped, see?
  - t=logic, c=stmt_singing_stopped, g=allison_wu, f=none | meters {'fear': 55, 'loyalty': 75} | 0.9s | repeats allison_wu
- **Detective:** Do it for Ace. He was your friend.
  - **Suspect** (afraid): He was my family, see? I told you what I saw, that's all I got to give you.
  - t=appeal, c=none, g=none, f=none | meters {'fear': 55, 'loyalty': 85} | 0.83s
- **Detective:** Who else goes behind your bar, Mickey?
  - **Suspect** (nervous): Only one person I ever let behind my bar was Vera Lane; she was here at 10:20 for her gin, just like always.
  - t=question, c=none, g=none, f=vera_behind_bar | meters {'fear': 55, 'loyalty': 85} | 0.97s | REVEALED vera_behind_bar; new evidence: ['stmt_vera_bar']

Revealed: ['tommy_fuse', 'gun_missing', 'vera_behind_bar']

## mickey_scared (Mickey Doyle)

_Lean on Mickey: he panics and shuts down without naming anyone._

- **Detective:** You found the body, Mickey. Start talking or you hang for it!
  - **Suspect** (afraid): Now hold on a minute, mister, you don't talk to me like that. I told you everything I know.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 75, 'loyalty': 40} | 0.81s
- **Detective:** You killed him, didn't you?
  - **Suspect** (nervous): Listen here, pal, you're talkin' crazy talk, I didn't do nothin' to Ace.
  - t=accuse, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 0.89s | rule: You are panicking. You keep saying you didn't see nothing, you were just pouring drinks, and you beg to be left alone.
- **Detective:** Was there a gun behind the bar?
  - **Suspect** (angry): No, mister, I told ya, nobody keeps a gun in this joint.
  - t=question, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 0.87s
- **Detective:** Who else goes behind your bar?
  - **Suspect** (afraid): Nobody, mister, just me and the customers, I was just pourin' drinks, I swear.
  - t=question, c=none, g=none, f=none | meters {'fear': 100, 'loyalty': 30} | 0.87s

Revealed: nothing

## graves_greed (Harlan Graves)

_Threats fail; a deal gets the motive, Logan's statement scares him into the cloakroom story, and a specific bluff at the end gets caught._

- **Detective:** Talk, Graves, or I'll have you in a cell tonight.
  - **Suspect** (smug): You're wasting both our time with these threats. Let's talk business, not threats.
  - t=threaten, c=none, g=none, f=none | meters {'fear': 8, 'greed': 50} | 0.72s
- **Detective:** With Ace gone the club is yours. Help me close this fast and it reopens clean.
  - **Suspect** (smug): A clean reopening is what I want, naturally. But my cooperation ain't cheap, Detective.
  - t=deal, c=none, g=none, f=none | meters {'fear': 8, 'greed': 70} | 0.8s
- **Detective:** A man of your standing knows how things really work here.
  - **Suspect** (smug): Well, I know how the bottom line works, Detective. That's the only thing that matters in this town.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 8, 'greed': 75} | 0.85s
- **Detective:** What was Ace going to announce tonight?
  - **Suspect** (smug): He was planning to announce Allison Wu as the new headliner, but I can tell you something else if you play your cards right.
  - t=question, c=none, g=none, f=contract_sold | meters {'fear': 8, 'greed': 75} | 0.88s | REVEALED contract_sold; new evidence: ['stmt_contract']
- **Detective:** *[presents stmt_cloakroom]*
  - **Suspect** (smug): Logan's eyesight must be failing, or he's mistaken. I was by my table, drinking, when the lights came back on.
  - t=logic, c=stmt_cloakroom, g=logan_wu, f=none | meters {'fear': 48, 'greed': 75} | 1.13s | repeats logan_wu
- **Detective:** So where were you really during the blackout?
  - **Suspect** (smug): I was in the cloakroom with Allison Wu, and she gave me Ace's real ledger; I promised to buy her contract once Ace was in court. I heard a woman's heels run up the back stairs in the dark.
  - t=question, c=none, g=none, f=cloakroom | meters {'fear': 48, 'greed': 75} | 1.15s | REVEALED cloakroom; new evidence: ['stmt_graves_cloakroom']
- **Detective:** Allison Wu told me she handed you Ace's ledger in the cloakroom.
  - **Suspect** (smug): She did, and I gave her my word. That ledger is worth more to me than any of this backroom drama.
  - t=logic, c=stmt_ledger, g=allison_wu, f=none | meters {'fear': 43, 'greed': 68} | 0.91s | bluff; repeats allison_wu

Revealed: ['contract_sold', 'cloakroom']

## tommy_brag_then_break (Tommy Mullen)

_Needle Tommy until he brags about the fuse, then use Mickey's statement to break him and get the name._

- **Detective:** Old wiring? I bet you couldn't fix a fuse box to save your life.
  - **Suspect** (angry): Hey, you don't know the things I know about wiring, pal. Let's just keep this chat moving.
  - t=challenge_pride, c=none, g=none, f=none | meters {'fear': 10, 'bravado': 82} | 0.88s
- **Detective:** A grease monkey like you? You couldn't kill the lights in this club if you tried.
  - **Suspect** (smug): Don't get ahead of yourself; I can handle anything that's wired up. I know this place better than anyone.
  - t=challenge_pride, c=none, g=none, f=none | meters {'fear': 10, 'bravado': 94} | 1.03s
- **Detective:** So what really happened with the lights?
  - **Suspect** (smug): Look, I pulled the main fuse at 11:00 on purpose and put it back at 11:04. Nobody kills the lights cleaner than you.
  - t=question, c=none, g=none, f=pulled_fuse | meters {'fear': 10, 'bravado': 94} | 1.08s | REVEALED pulled_fuse; new evidence: ['stmt_fuse_pulled']
- **Detective:** *[presents stmt_tommy_fuse]*
  - **Suspect** (angry): Mickey's got it twisted; I was in the alley fixing the line, not running in from the back like some rat.
  - t=logic, c=stmt_fuse_pulled, g=mickey_doyle, f=none | meters {'fear': 45, 'bravado': 54} | 1.18s | repeats mickey_doyle
- **Detective:** You were at the fuse box, not in the alley. That makes you part of a murder, Tommy.
  - **Suspect** (nervous): I told you I was fixing the wiring; that's it. You got your story wrong.
  - t=accuse (expected logic), c=none, g=none, f=none | meters {'fear': 55, 'bravado': 49} | 0.86s
- **Detective:** Who asked you to cut the lights?
  - **Suspect** (afraid): It was Vera Lane who paid me twenty dollars at nine o'clock to cut the lights at eleven sharp for four minutes.
  - t=question, c=none, g=none, f=who_paid | meters {'fear': 55, 'bravado': 49} | 0.97s | REVEALED who_paid; new evidence: ['stmt_vera_paid']

Revealed: ['pulled_fuse', 'who_paid']

## allison_ambition (Allison Wu)

_Feed Allison's ambition and keep her calm: she admits the ledger and says Vera's singing stopped._

- **Detective:** Miss Wu. I hear you're the voice everyone's talking about.
  - **Suspect** (calm): I try to keep up with the standards of the club, Detective. The music demands perfection.
  - t=flattery, c=none, g=none, f=none | meters {'fear': 40, 'ambition': 63} | 0.78s
- **Detective:** The new owners will need a headliner. I could put in a good word for you.
  - **Suspect** (hopeful): That would be very kind of you, Detective. I hope my talent speaks for itself soon enough.
  - t=reassure (expected deal), c=none, g=none, f=none | meters {'fear': 30, 'ambition': 73} | 0.85s
- **Detective:** Where were you during the blackout?
  - **Suspect** (nervous): I wasn't in my dressing room, Detective; I was in the cloakroom with Harlan Graves, giving him Ace's real ledger, which I took from the office at ten.
  - t=question, c=none, g=none, f=ledger_theft | meters {'fear': 30, 'ambition': 73} | 0.99s | REVEALED ledger_theft; new evidence: ['stmt_ledger']
- **Detective:** Your husband won't hear any of this from me. You have my word.
  - **Suspect** (calm): I appreciate your assurance, Detective. I trust your discretion completely.
  - t=reassure, c=none, g=none, f=none | meters {'fear': 20, 'ambition': 83} | 0.88s
- **Detective:** What did you hear of Vera's set while the lights were out?
  - **Suspect** (smug): Vera's voice stopped the moment the lights went out; the band vamped for four minutes, and she never takes a break in 'Black Spade Blues'. And in the dark, heels ran past the cloakroom toward the back stairs.
  - t=question, c=none, g=none, f=singing_stopped | meters {'fear': 20, 'ambition': 83} | 1.15s | REVEALED singing_stopped; new evidence: ['stmt_singing_stopped']

Revealed: ['ledger_theft', 'singing_stopped']

## logan_temper (Logan Wu)

_Quote Vera to scare Logan into talking, then provoke him with the cloakroom: he walks out and takes Allison._

- **Detective:** Mr Wu. Thank you for your patience tonight.
  - **Suspect** (calm): It is my pleasure to assist in this inquiry, Detective. I trust the evening is proving less taxing for you.
  - t=small_talk, c=none, g=none, f=none | meters {'fear': 20, 'temper': 27} | 0.77s
- **Detective:** Vera Lane says she saw you come out of the back hall when the lights came back.
  - **Suspect** (calm): I assure you, Detective, I remained at my table near the stage the entire time. I did not approach the back hall.
  - t=question (expected logic), c=stmt_vera_went_up, g=vera_lane, f=none | meters {'fear': 35, 'temper': 42} | 1.07s | bluff; repeats vera_lane
- **Detective:** You told the officers you never left your table. Which is it?
  - **Suspect** (angry): I have told you the truth, Detective, and I expect you to accept it. My word is my bond, sir.
  - t=challenge_pride (expected logic), c=none, g=none, f=none | meters {'fear': 40, 'temper': 57} | 1.22s
- **Detective:** Where did you go during the blackout?
  - **Suspect** (angry): I did not stay at my table. At 11:01 you went to find Allison, and her dressing room was empty. In the dark back hall a woman brushed past you heading for the back stairs, wearing gardenia perfume; Allison wears jasmine.
  - t=question, c=none, g=none, f=went_looking | meters {'fear': 40, 'temper': 57} | 1.45s | REVEALED went_looking; new evidence: ['stmt_gardenia', 'stmt_cloakroom']
- **Detective:** *[presents stmt_ledger]*
  - **Suspect** (calm): Allison admits to being in the cloakroom with Graves, and she gave him the ledger. That is a matter for your investigation, Detective.
  - t=logic, c=stmt_cloakroom, g=allison_wu, f=none | meters {'fear': 55, 'temper': 92} | 1.37s | repeats allison_wu; rule: You have had enough. You stand, take your hat, and leave the club, and you are taking your wife home with you.
- **Detective:** Some husband. Your wife sneaks off with Graves and you sit there sweating.
  - _(interview closed: suspect has walked out)_
- **Detective:** Sit down, Mr Wu, I'm not finished.
  - _(interview closed: suspect has walked out)_

Revealed: ['went_looking']
