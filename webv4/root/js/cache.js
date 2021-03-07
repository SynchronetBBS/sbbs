(() => {

    const schemas = {
        avatars: {
            avatars: {
                keyPath: 'user',
            },
        },
        forum: {
            groups: {
                keyPath: 'index',
            },
            subs: {
                keyPath: 'code',
                indexes: {
                    keyPath: 'grp_index',
                }
            },
            threads: {
                keyPath: 'id',
                indexes: {
                    keyPath: 'sub',
                },
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
                        for (const i of Object.values(v.indexes)) {
                            os.createIndex(i, i);
                        }
                    }
                }
            }
        });
    }

    async function getDataByIndex(schema, store, index, condition) {
        const db = await initDB(schema);
        return new Promise(res => {
            const ret = [];
            const i = db.transaction([store], 'readonly').objectStore(store).index(index);
            i.openCursor().onsuccess = evt => {
                const cursor = evt.target.result;
                if (cursor) {
                    if (condition(cursor.value)) ret.push(cursor.value);
                    // if (cursor.value[index] === query[index]) ret.push(cursor.value);
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

    window.sbbs = {
        avatars: {
            get: user => getData('avatars', 'avatars', user),
            set: user => setData('avatars', 'avatars', user),
        },
        forum: {
            getGroup: data => getData('forum', 'groups', data),
            setGroup: data => setData('forum', 'groups', data),
            getGroups: () => getData('forum', 'groups'),
            getSub: data => getData('forum', 'subs', data),
            setSub: data => setData('forum', 'subs', data),
            getSubs: c => getDataByIndex('forum', 'subs', 'grp_index', c),
            getThread: data => getData('forum', 'threads', data),
            setThread: data => setData('forum', 'threads', data),
            getThreads: q => getDataByIndex('forum', 'threads', 'sub', q),
        },
    }

})();