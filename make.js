const wilu = require('wilu');
const pkg = require('./package.json');

(async function () {
	try {
		await wilu(pkg);
	} catch(e) {
		console.error(e);
	}
})();
