const express = require('express');
const axios = require('axios');
const dotenv = require('dotenv');
dotenv.config();
const app = express();

/**
 * Health check
 */
app.get('/', (req, res) => {
  res.send('Hello World!')
})

/**
 * Link for toggling item
 */
app.get('/items/:item', (req, res) => {
  const item = req.params.item;
  const config = {
    method: 'get',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items/${item}_Power`
  };


  axios(config)
  .then(async (response) => {
    if (response.data.state === "NULL") {
      console.error(`${item} does not exist.`);
      res.send(`${item} does not exist.`);
    } else {
      const curState = response.data.state;
      console.log(`State of ${item} is ${curState}`);

      let newState = "OFF";
      if (curState === "OFF") newState = "ON";

      await setItemState(item, newState);
      res.send(`${item} is ${curState}. Turning it ${newState}`);
    }
  })
  .catch((error) => {
    console.log(error);
    res.send(error);
  });
});

/**
 * Link for blinking multiple items
 */
app.get('/blink', async (req, res) => {
  const items = ['Kitchen', 'Livingroom', 'VinnisPlug', 'MainHallway'];
  console.log(`Blinking ${items}`);

  setItemsState(items, 'OFF');
  await delay(2000);
  setItemsState(items, 'ON');
  await delay(2000);
  setItemsState(items, 'OFF');
  await delay(2000);
  setItemsState(items, 'ON');
  await delay(2000);

  res.send('And it Blinked!');
});

/**
 * Link for getting all items' attributes
 */
app.get('/items', (req, res) => {
  const config = {
    method: 'get',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items`
  };

  axios(config)
  .then((response) => {
    res.send(response.data);
  })
  .catch((error) => {
    console.log(error);
    res.send(error);
  });
});

/**
 * Set state for multiple items
 * @param {*} items
 * @param {*} newState
 */
async function setItemsState(items, newState) {
  console.log(`Turning all ${newState}`);
  items.forEach(async (item) => {
    await setItemState(item, newState);
  });
}

/**
 * Set state for single item
 * @param {*} item
 * @param {*} newState
 */
async function setItemState(item, newState) {
  console.log(`Turning ${item} ${newState}`);
  const config = {
    method: 'post',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items/${item}`,
    data: newState,
    headers: {
      'Content-Type': 'text/plain',
      'Cookie': 'X-OPENHAB-AUTH-HEADER=true;'
    }
  };

  axios(config)
  .then(function (response) {
    console.log(`${item} turned ${newState}`);
  })
  .catch(function (error) {
    console.log(error);
  });
}

/**
 * Helper for explicit delay
 * @param {*} delayInms
 */
function delay(delayInms) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve(2);
    }, delayInms);
  });
}

app.listen(process.env.PORT, () => console.log(`Server running on port ${process.env.PORT}`));
