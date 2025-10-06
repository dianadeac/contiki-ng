/* A simple log file generator script */

TIMEOUT(3600000); /* 3600 seconds or 1 hour */

log.log("Starting COOJA logger\n");

timeout_function = function () {
  log.log("Script timed out.\n");
  log.testOK();
};

var moved = false;

while (true) {
  if (msg) {
    log.log(time + " " + id + " " + msg + "\n");
  }
  if (!moved && time >= 120000000) {
    var node = sim.getMoteWithID(14);
    var pos = node.getInterfaces().getPosition();
    pos.setCoordinates(300.0, 300.0, 0.0); // x, y, z
    log.log("Moved node 14 at time " + time + " to position (300.0, 300.0)\n");
    moved = true;
  }


  YIELD();
}
