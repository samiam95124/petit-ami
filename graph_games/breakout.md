# Playing Breakout

Knock every brick out of the wall by bouncing the ball off your paddle.
The ball serves itself from the lower left a moment after each point
begins; keep it in the air. If it gets past your paddle and reaches the
bottom, the ball is lost, and a new one serves after a short wait.

Each brick knocked out scores a point. When the whole wall is cleared, a
fanfare plays and a fresh wall is built, and the game continues with your
score intact.

The score is shown at the top of the window.

# Moving the Paddle

There are three ways to drive the paddle, and they hand off to each other
naturally.

## Keyboard

The left and right arrow keys step the paddle. Holding a key repeats the
step at the keyboard repeat rate.

## Mouse capture

The paddle can be captured to the mouse, which is the smoothest way to
play:

- Press and hold the mouse button on the paddle for about a second. The
  paddle turns yellow: it is captured, and follows the mouse from side to
  side. The button is not needed while playing.
- To let go, press and hold the button again for about a second. The
  paddle turns green and is released.
- Touching an arrow key, or moving the joystick, also releases the
  paddle instantly.

## Joystick

If a joystick is present, deflecting it positions the paddle across the
field. The paddle follows the stick only while the stick itself moves,
so an idle joystick never fights the other controls.

# Aiming the Ball

The paddle acts like a real paddle: where the ball strikes it decides
where the ball goes.

- A ball taken at the center returns near the mirror of its approach.
- The farther from center it is taken, the more it is turned toward
  that side; the edges throw the sharpest angles.
- The ball's speed never changes, and a shot is never allowed to go
  flat, so play always advances.

Catching the ball off-center on purpose is how you cut angles into the
corners and dig out the last bricks.

# Keys

- Left and right arrows: move the paddle.
- Any function key: restart the game.
- The Game menu starts a new game or exits; the Help menu shows this
  help and the program's pedigree.
