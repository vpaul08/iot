const express = require('express');
const axios = require('axios');
const dotenv = require('dotenv');
dotenv.config();
const app = express();

app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.get('/items/:item', (req, res) => {
  res.send(req.params);
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
