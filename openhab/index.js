const express = require('express');
const axios = require('axios');
const dotenv = require('dotenv');
dotenv.config();
const app = express();

app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.get('/items/:item', (req, res) => {
  var config = {
    method: 'get',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items/${req.params.item}`
  };

  axios(config)
  .then((response) => {
    if (response.data.state === "NULL") {
      res.send(`${req.params.item} does not exist.`);
    } else {
      res.send(`${req.params.item} is ${response.data.state}`);
    }

  })
  .catch((error) => {
    res.send(error);
  });
});

app.get('/items', (req, res) => {

  var config = {
    method: 'get',
    url: `https://${process.env.USERNAME}:${process.env.PASSWORD}@${process.env.OPENHAB_HOST}/items`
  };

  axios(config)
  .then((response) => {
    res.send(response.data);
  })
  .catch((error) => {
    res.send(error);
  });
});

app.listen(process.env.PORT, () => console.log(`Server running on port ${process.env.PORT}`));
