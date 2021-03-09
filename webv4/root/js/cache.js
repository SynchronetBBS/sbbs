(() => {

    const schemas = {
        avatars: {
            users: {
                keyPath: 'user',
            },
        },
    };

    function initDB(schema) {
        return new Promise((res, rej) => {
            const request = indexedDB.open(schema, 1);
            request.onerror = err => {
                console.error('Cache init error', err);
                rej(err);
            }
            request.onsuccess = evt => {
                evt.target.result.onerror = evt => console.error('Cache error', evt.target.errorCode);
                res(evt.target.result);
            }
            request.onupgradeneeded = function () {
                const db = this.result;
                for (const [k, v] of Object.entries(schemas[schema])) {
                    const os = db.createObjectStore(k, { keyPath: v.keyPath });
                    if (v.indexes !== undefined) {
                        for (const i of v.indexes) {
                            os.createIndex(i, i);
                        }
                    }
                }
            }
        });
    }

    async function getDataByIndex(schema, store, index, predicate, forEach) {
        const db = await initDB(schema);
        return new Promise(res => {
            const ret = [];
            const i = db.transaction([store], 'readonly').objectStore(store).index(index);
            i.openCursor().onsuccess = evt => {
                const cursor = evt.target.result;
                if (cursor) {
                    if (predicate(cursor.value)) {
                        if (typeof forEach === 'function') forEach(cursor.value);
                        ret.push(cursor.value);
                    }
                    cursor.continue();
                } else {
                    res(ret);
                }
            }
        });
    }

    async function getData(schema, store, key) {
        const db = await initDB(schema);
        return new Promise(res => {
            const s = db.transaction([store]).objectStore(store);
            let r;
            if (key === undefined) {
                r = s.getAll(key);
            } else {
                r = s.get(key);
            }
            r.onerror = evt => {
                console.error(evt);
                res();
            };
            r.onsuccess = () => res(r.result);
        });
    }

    async function setData(schema, store, data) {
        const db = await initDB(schema);
        return new Promise(res => {
            const r = db.transaction([store], 'readwrite').objectStore(store).put(data);
            r.onerror = evt => {
                console.error(evt);
                res(false);
            }
            r.onsuccess = () => res(true);
        });
    }

    // Upsert; returns false if no change or failed to upsert; returns true if change or new record
    async function updateData(schema, store, data) {
        const d = await getData(schema, store, data[schemas[schema][store].keyPath]);
        if (JSON.stringify(data) === JSON.stringify(d)) return false;
        return setData(schema, store, data);
    }

    window.sbbs = {};
    for (const schema in schemas) {
        window.sbbs[schema] = {};
        for (const store in schemas[schema]) {
            window.sbbs[schema][store] = {
                get: key => getData(schema, store, key),
                getAll: () => getData(schema, store),
                getAllByIndex: (index, predicate, forEach) => getDataByIndex(schema, store, index, predicate, forEach),
                set: data => setData(schema, store, data),
                update: data => updateData(schema, store, data),
            };
        }
    }
 
})();