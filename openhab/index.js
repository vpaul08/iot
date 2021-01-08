const express = require('express');
const axios = require('axios');
const dotenv = require('dotenv');
dotenv.config();
const app = express();

app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.get('/items/:item', (req, res) => {
  const config = {
    method: 'get',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items/${req.params.item}_Power`
  };
  const item = req.params.item;

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

      await toggle(item, newState);
      res.send(`${item} is ${curState}. Turning it ${newState}`);
    }
  })
  .catch((error) => {
    console.log(error);
    res.send(error);
  });
});

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

async function toggle(item, newState) {
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
    console.log(JSON.stringify(response.data));
  })
  .catch(function (error) {
    console.log(error);
  });
}

app.listen(process.env.PORT, () => console.log(`Server running on port ${process.env.PORT}`));
