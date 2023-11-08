import sc2_replay_reader
import matplotlib.pyplot as plt

x = sc2_replay_reader.ReplayDatabase("4.9.2_replays.SC2Replays")
y = x.getEntry(0)
print(y.buildable[0].data.dtype)

print(dir(y))
print(y.heightMap)
print(y.replayHash)
print(y.playerId)
print(y.playerRace)
print(y.playerResult)
print(y.playerMMR)
print(y.playerAPM)
print(y.visibility[0])
print(y.creep[0])
print(y.player_relative[0])
print(y.alerts[0])
print(y.buildable[0])
print(y.pathable[0])
print(y.actions[0][0].target)
print(y.actions[0][0].target_type)

print(y.units[0])

plt.imshow(y.buildable[0].data)
plt.show()
