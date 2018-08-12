const native = require('./build/node-addon-api-async-tsf/lib/async_tsf');
console.dir(native);


class Example {
  constructor() {
    native.initialize(this);
  }

  start() {
    native.start();
  }

  async sleep(ms) {
    try {
      await new Promise(function(success) { setTimeout(success, ms); });
    } catch (e) {
      throw(e);
    }
  }

  async func(parameter) {
    try {
      console.log('in javascript Example::func', parameter);
      await this.sleep(2000);
      return 14;
    } catch(e) {
      console.error(e);
      throw(e);
    }
  }  
}

const ex = new Example();
ex.start();
