const express = require('express');
const dotenv = require('dotenv');
dotenv.config();
const app = express();
app.set('views', './views')
app.set('view engine', 'jade')

app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.get('/360vr', (req, res) => {
  res.render('360vr');
})

app.listen(process.env.PORT, () => console.log(`Server running on port ${process.env.PORT}`));
