#include <msp430.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <libTimer.h>
#include <p2switches.h>
#include "buzzer.h"
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6


u_char player1Score = '0';
u_char player2Score = '0'; //Player scores
static int state = 0; //End the game
AbRect rect = {abRectGetBounds, abRectCheck, {2,10}};
Region fence = {{10,20}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}};

//Draw the Paddles and Ball
AbRectOutline fieldOutline =
  {
    abRectOutlineGetBounds, abRectOutlineCheck,
    {screenWidth/2-5, screenHeight/2-15} //testing values
  };

Layer fieldLayer =
  {
    (AbShape *)&fieldOutline,
    {screenWidth/2, screenHeight/2},
    {0,0}, {0,0},
    COLOR_RED,
  };

/*
  layer1 == Player 1 Paddle
  layer2 == Player 2 Paddle
  layer3 == Ball
*/

Layer layer3 = //Ball
  {
    (AbShape *) &circle4,
    {(screenWidth/2), (screenHeight/2)},
    {0,0}, {0,0},
    COLOR_BLACK, &fieldLayer
  };

Layer layer1 = //Paddle for player 1
  {
    (AbShape *) &rect, //Rectangle shape
    {screenWidth/2-55, screenHeight/3},
    {0,0}, {0,0},
    COLOR_RED,
    &layer3 //ball layer
  };

Layer layer2 = //Same as player 1, just move it to the other side
  {
    (AbShape *) &rect,
    {screenWidth/2+55, screenHeight/3+5},
    {0,0}, {0,0},
    COLOR_RED,
    &layer1 //p1 layer
  };

//Now to draw the field and layer
//Add Moving layers to draw

typedef struct MovLayer_s //Using this from shape-motion-demo
{
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer ml1 = { &layer1, {0,3}, 0}; //P1 Paddle
MovLayer ml2 = { &layer2, {0,3}, 0}; //P2 Paddle
MovLayer ml3 = { &layer3, {2,4}, 0}; //Ball

//Move the Ball

void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;
  and_sr(~8);/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1],
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer;
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break;
	  } /*if probe check*/
	} //for checking all alyers at col, row
	lcd_writeColor(color);
      } //for col
    } //for row
  } // for moving layer being updated
}

void moveUp(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Sub(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
	buzzer_set_period(720);//make a sound for collision
      }/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

void moveDown(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity =-ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
	buzzer_set_period(720);//make a sound for collision
      }/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}

void moveBall(MovLayer *ml, Region *fence1, MovLayer *ml2, MovLayer *ml3)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  int velocity;
  buzzer_set_period(0); //No Sound

  for (; ml; ml = ml->next)
    {
      vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
      abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
      for(axis = 0; axis < 2; axis ++)
	{
	  if((shapeBoundary.topLeft.axes[axis] < fence1->topLeft.axes[axis]) || //Just kill and yank to make it work with paddles
	     (shapeBoundary.botRight.axes[axis] > fence1->botRight.axes[axis]) ||
	     (abShapeCheck(ml3->layer->abShape, &ml3->layer->posNext, &ml->layer->posNext)) ||
	     (abShapeCheck(ml2->layer->abShape, &ml2->layer->posNext, &ml->layer->posNext)))
	    {
	      velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	      newPos.axes[axis] += (2*velocity);
	      buzzer_set_period(300);
	    }

	  else if((shapeBoundary.topLeft.axes[0] < fence1->topLeft.axes[0]))
	    {
	      newPos.axes[0] = screenWidth/2;
	      newPos.axes[1] = screenHeight/2;
	      player2Score = player2Score - 255; //P2 Score
	    }

	  else if((shapeBoundary.botRight.axes[0] > fence1->botRight.axes[0]))
	    {
	      newPos.axes[0] = screenWidth/2;
	      newPos.axes[1] = screenHeight/2;
	      player1Score = player1Score - 255; //P1 score
	    }

	  if(player1Score == '5' || player2Score == '5'){
	    buzzer_set_period(0);
	    state = 1;
	  }
	} /**< for axis */
      ml->layer->posNext = newPos;
    } /**< for ml */
}

	       
//Move handler to handle interrupts and use it for endgame

u_int bgColor = COLOR_BLUE;
int redrawScreen = 1;
Region fieldFence;

void wdt_c_handler()
{
  static short count = 0;

  P1OUT |= GREEN_LED;      /**< Green LED on when cpu on */
  count ++;

  u_int switches = p2sw_read();

  if(count == 10)
    {
      switch(state)
	{
	case 0:
	  moveBall(&ml3, &fieldFence, &ml1, &ml2);
	  break;

	case 1: //Case for when the game reaches 3, announces winner winner

	  layerDraw(&layer2);



	  if(player1Score > player2Score)
	    drawString5x7(20, 50, " Player 1 Victory ", COLOR_WHITE, COLOR_BLUE);

	  else if(player1Score < player2Score)
	    drawString5x7(20, 50, " Player 2 Victory ", COLOR_WHITE, COLOR_BLUE);

	  break;
	}

      /*
        Paddle Controls
	Button 1: P1 Up
	Button 2: P1 Down
	Button 3: P2 Up
	Button 4: P2 Down
      */

      if(switches & (1<<3))//Button 4
	moveUp(&ml2, &fieldFence);

      if(switches & (1<<2))//Button 3
	moveDown(&ml2, &fieldFence);

      if(switches & (1<<1))//Button 2
	moveUp(&ml1, &fieldFence);

      if(switches & (1<<0))//Button 1
	moveDown(&ml1, &fieldFence);

      redrawScreen = 1;
      count = 0;
    }
  P1OUT &= ~GREEN_LED;    /**< Green LED off when cpu off */
}


//Game execute
void main()
{
  P1DIR |= GREEN_LED;//Green led on when CPU on
  P1OUT |= GREEN_LED;

  configureClocks(); //initialize clocks
  lcd_init(); //initialize lcd
  buzzer_init(); //initialize buzzer
  p2sw_init(15); //initialize switches
  layerInit(&layer2); //Passes the first element from a MoveLayer LL to initilize shapes
  layerDraw(&layer2); //Passes the first element from a MoveLayer LL to draw shapes
  layerGetBounds(&fieldLayer, &fieldFence);
  enableWDTInterrupts();      // enable periodic interrupt
  or_sr(0x8);              // GIE (enable interrupts)

  u_int switches = p2sw_read();

  for(;;)//Dont get this part, would a while loop just work? Copied from shape demo
    {
      while (!redrawScreen)
	{ // Pause CPU if screen doesn't need updating
	  P1OUT &= ~GREEN_LED; // Green led off witHo CPU
	  or_sr(0x10); //< CPU OFF
	}
      P1OUT |= GREEN_LED; // Green led on when CPU on
      redrawScreen = 0;

      movLayerDraw(&ml3, &layer2);
      movLayerDraw(&ml2, &layer2);
      movLayerDraw(&ml1, &layer2);

      //P1 Score
      drawChar5x7(5, 150, player1Score, COLOR_WHITE, COLOR_BLUE);

      //P2 Score
      drawChar5x7(115, 150, player2Score, COLOR_WHITE, COLOR_BLUE);

      //Mother of all Pongs
      drawString5x7(5, 5, "===PING PONG===", COLOR_WHITE, COLOR_RED);

      //Glance at Victory
      drawString5x7(25, 150, "<===SCORES===>", COLOR_WHITE, COLOR_RED);
    }
}
