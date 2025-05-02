const DEBUG = false;  // set to false to disable debug logging
const ROLLING_WINDOW_HOURS = 6; // 6 hr rolling window
const SMOOTH_CURVES = true; // curve smoothing

function debugLog(message, data) {
    if (DEBUG) {
        console.log(`[DEBUG] ${message}`, data ? data : '');
    }
}

// define colors for each event
const eventColors = {
    leftclicks: '#90EE90',    // light green
    rightclicks: '#98FB98',   // pale green
    middleclicks: '#98FF98',  // mint green
    keypresses: '#7FFF00',    // chartreuse
    mousemoves: '#00FF7F'     // spring green
};

let currentData = [];
let chartUpdateInterval;

// init data load
loadData();

// auto-refresh
setInterval(loadData, 60000); // refresh every min

function loadData() {
    // cumulative data display
    fetch('./cumulative_data.csv')
        .then(response => response.text())
        .then(data => {
            debugLog('Cumulative data:', data);
            const lines = data.split('\n');
            if (lines.length > 1) {
                const [keypresses, mousemoves, leftclicks, rightclicks] = lines[1].split(',');
                
                document.getElementById('keypresses').textContent = keypresses;
                document.getElementById('mousemoves').textContent = mousemoves + 'm';
                document.getElementById('leftclicks').textContent = leftclicks;
                document.getElementById('rightclicks').textContent = rightclicks;
            }
        })
        .catch(error => console.error('error loading cumulative data:', error));

    // fetch past 24h data + generate chart
    fetch('./past_24_hours_data.csv')
        .then(response => response.text())
        .then(data => {
            const parsedData = parseCSV(data);
            currentData = parsedData;
            renderChart(parsedData);
        })
        .catch(error => console.error('error loading past 24 hours data:', error));
}

function parseCSV(data) {
    debugLog('Raw CSV data:', data);
    const lines = data.split('\n')
        .filter(line => line.trim() && !line.startsWith('cumulative') && !line.startsWith('timestamp'));
    if (lines.length === 0) return [];
    
    // parse all data points
    let parsedData = lines.map(line => {
        const [timestamp, keypresses, mousemoves, leftclicks, rightclicks, middleclicks] = line.split(',');

        // skip lines with invalid data
        if (!timestamp || isNaN(parseInt(timestamp))) return null;

        return {
            timestamp: new Date(parseInt(timestamp) * 1000),
            keypresses: parseInt(keypresses) || 0,
            mousemoves: parseFloat(mousemoves.replace(' meters', '')) || 0,
            leftclicks: parseInt(leftclicks) || 0,
            rightclicks: parseInt(rightclicks) || 0,
            middleclicks: parseInt(middleclicks) || 0
        };
    }).filter(d => d !== null && !isNaN(d.timestamp.getTime()));

    // sort by timestamp
    parsedData = parsedData.sort((a, b) => a.timestamp - b.timestamp);

    // rolling window: only keep data from last x hours
    const now = new Date();
    const cutoffTime = new Date(now.getTime() - (ROLLING_WINDOW_HOURS * 60 * 60 * 1000));
    parsedData = parsedData.filter(d => d.timestamp >= cutoffTime);

    debugLog('filtered data points:', parsedData);
    return parsedData;
}

function updateCurrentActivityTooltip(data) {
    if (!data || data.length === 0) return;
    const latestData = data[data.length - 1];

    // create/update current display
    const currentActivity = document.getElementById('current-activity') || document.createElement('div');
    currentActivity.id = 'current-activity';
    currentActivity.innerHTML = `
        <div style="position: absolute; bottom: 10px; left: 10px; background: rgba(0, 100, 0, 0.7); 
             border: 1px solid #0f0; padding: 10px; font-family: monospace; color: #0f0; z-index: 100;">
            <div style="font-weight: bold; border-bottom: 1px solid #0f0; margin-bottom: 5px;">
                ${latestData.timestamp.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'})}
            </div>
            <div>keypresses: ${latestData.keypresses}</div>
            <div>mouse moves: ${latestData.mousemoves.toFixed(2)}m</div>
            <div>left clicks: ${latestData.leftclicks}</div>
            <div>right clicks: ${latestData.rightclicks}</div>
        </div>
    `;

    // add to chart if it doesn't exist
    const chartContainer = document.getElementById('chart');
    if (!document.getElementById('current-activity')) {
        chartContainer.style.position = 'relative';
        chartContainer.appendChild(currentActivity);
    }
}

function renderChart(data) {
    if (!data || data.length === 0) {
        debugLog('o data to render');
        return;
    }
    debugLog('rendering chart with data points:', data.length);
    
    // clear existing chart and tooltips
    d3.select('#chart').html('');
    d3.selectAll('.tooltip').remove();

    // setup dimensions
    const chartContainer = d3.select('#chart');
    const containerWidth = parseInt(chartContainer.style('width'));
    const containerHeight = parseInt(chartContainer.style('height'));
    const margin = {top: 20, right: 20, bottom: 30, left: 50};
    const width = (containerWidth || 800) - margin.left - margin.right;
    const height = (containerHeight || 400) - margin.top - margin.bottom;

    // create SVG
    const svg = chartContainer
        .append('svg')
        .style('width', '100%')
        .style('height', '100%')
        .attr('viewBox', `0 0 ${width + margin.left + margin.right} ${height + margin.top + margin.bottom}`)
        .append('g')
        .attr('transform', `translate(${margin.left},${margin.top})`);

    // find gaps
    const MAX_GAP_MINUTES = 15;
    let dataWithGaps = [];

    // find gaps + insert 0 (not null) to make lines go to 0
    for (let i = 0; i < data.length; i++) {
        dataWithGaps.push(data[i]);

        if (i < data.length - 1) {
            const currentTime = data[i].timestamp.getTime();
            const nextTime = data[i+1].timestamp.getTime();
            const diffMinutes = (nextTime - currentTime) / (1000 * 60);

            if (diffMinutes > MAX_GAP_MINUTES) {
                // insert 0 point right after current time 
                dataWithGaps.push({
                    timestamp: new Date(currentTime + 60000),
                    keypresses: 0, 
                    mousemoves: 0,
                    leftclicks: 0,
                    rightclicks: 0,
                    middleclicks: 0
                });
                // insert null at next time - 1
                dataWithGaps.push({
                    timestamp: new Date(nextTime - 60000),
                    keypresses: 0,
                    mousemoves: 0,
                    leftclicks: 0,
                    rightclicks: 0,
                    middleclicks: 0
                });
            }
        }
    }
    
    // setup scales (with gaps)
    //const x = d3.scaleTime()
    //    .domain(d3.extent(dataWithGaps, d => d.timestamp))
    //    .range([0, width]);
    
    // this part handles missing data
    // use a fixed time range for the last ROLLING_WINDOW_HOURS
    const now = new Date();
    const startTime = new Date(now.getTime() - (ROLLING_WINDOW_HOURS * 60 * 60 * 1000));

    // if no data or limited date
    // we should have 0 points at boundaries
    if (dataWithGaps < 2) {
        // 0 point at window start
        dataWithGaps.push({
            timestamp: startTime,
            keypresses: 0,
            mousemoves: 0,
            leftclicks: 0,
            rightclicks: 0,
            middleclicks: 0
        });

        // 0 point at current time
        dataWithGaps.push({
            timestamp: now,
            keypresses: 0,
            mousemoves: 0,
            leftclicks: 0,
            rightclicks: 0,
            middleclicks: 0
        });
    } else {
        // time boundaries by adding 0s if needed
        const earliestTime = dataWithGaps[0].timestamp;
        const latestTime = dataWithGaps[dataWithGaps.length - 1].timestamp;

        // if earliest data point is after window start
        // add 0 point
        if (earliestTime > startTime) {
            dataWithGaps.unshift({
                timestamp: startTime,
                keypresses: 0,
                mousemoves: 0,
                leftclicks: 0,
                rightclicks: 0,
                middleclicks: 0
            });
        }

        // if latest data point is before current time
        // add 0 point
        //if (latestTime < now) {
        //    dataWithGaps.push({
        //        timestamp: now,
        //        keypresses: 0,
        //        mousemoves: 0,
        //        leftclicks: 0,
        //        rightclicks: 0,
        //        middleclicks: 0
        //    });
        //}
    }

    // set x scale with fixed boundaries
    const x = d3.scaleTime()
        .domain([startTime, now])
        .range([0, width]);

    const y = d3.scaleLinear()
        .domain([0, d3.max(dataWithGaps, d => 
            Math.max(d.keypresses || 0, d.mousemoves || 0, d.leftclicks || 0, d.rightclicks || 0)
        ) * 1.1 || 10])
        .range([height, 0]);

    // create tooltip once
    const tooltip = d3.select('body')
        .append('div')
        .attr('class', 'tooltip')
        .style('opacity', 0)
        .style('position', 'absolute')
        .style('pointer-events', 'none');


    // draw grid
    function createGrid() {
        svg.append('g')
            .attr('class', 'grid')
            .call(d3.axisLeft(y)
                .ticks(5)
                .tickSize(-width)
                .tickFormat(''))
            .style('stroke', '#0f0')
            .style('stroke-opacity', 0.1);

        svg.append('g')
            .attr('class', 'grid')
            .attr('transform', `translate(0,${height})`)
            .call(d3.axisBottom(x)
                .ticks(d3.timeHour.every(1))
                .tickSize(-height)
                .tickFormat(''))
            .style('stroke', '#0f0')
            .style('stroke-opacity', 0.1);
    }

    // draw axes
    function createAxes() {
        const xAxis = d3.axisBottom(x)
            .ticks(d3.timeHour.every(1))
            .tickFormat(d => {
                const hours = d.getHours();
                const minutes = d.getMinutes();
                const ampm = hours >= 12 ? 'PM' : 'AM';
                const hour12 = hours % 12 || 12;
                
                // only show h:m for even hours (h only otherwise)
                if (minutes === 0) {
                    return `${hour12}${ampm}`;
                } else if (minutes === 30) {
                    return `${hour12}:30`;
                }
                return '';
            })
            .tickSizeOuter(0);

        svg.append('g')
            .attr('transform', `translate(0,${height})`)
            .call(xAxis)
            .style('color', '#0f0')
            .style('font-family', 'monospace')
            .style('font-size', '12px');

        svg.append('g')
            .attr('class', 'y-axis')
            .call(d3.axisLeft(y).ticks(5))
            .style('color', '#0f0');
    }

    // draw data lines and points
    function drawDataLines() {
        const filteredEventColors = Object.fromEntries(
            Object.entries(eventColors).filter(([key]) => key !== 'middleclicks')
        );

        Object.entries(filteredEventColors).forEach(([metric, color]) => {
            // draw line
            const line = d3.line()
                .x(d => x(d.timestamp))
                .y(d => y(d[metric] || 0))
                .defined(d => d[metric] !== null) // skip null values
                .curve(SMOOTH_CURVES ? d3.curveMonotoneX : d3.curveLinear);
                //.curve(d3.curveLinear);

            svg.append('path')
                .datum(dataWithGaps)
                .attr('fill', 'none')
                .attr('stroke', color)
                .attr('stroke-width', 2)
                .attr('d', line);

            // draw points
            if (data.length < 50) {
                svg.selectAll(`dot-${metric}`)
                    .data(data)
                    .enter()
                    .append('circle')
                    .attr('cx', d => x(d.timestamp))
                    .attr('cy', d => y(d[metric] || 0))
                    .attr('r', 3)
                    .attr('fill', color)
                    .attr('opacity', 0.7);
            }
        });
    }

    // create mouse tracking overlay
    function createMouseTracking() {
        // voronoi overlay
        //const delaunay = d3.Delaunay.from(data, d => x(d.timestamp), d => y(d.keypresses));
        //const voronoi = delaunay.voronoi([0, 0, width, height]);

        const overlay = svg.append('rect')
            .attr('class', 'overlay')
            .attr('width', width)
            .attr('height', height)
            .attr('fill', 'none')
            .style('pointer-events', 'all');

        const verticalLine = svg.append('line')
            .attr('class', 'tracking-line')
            .attr('y1', 0)
            .attr('y2', height)
            .style('stroke', '#0f0')
            .style('stroke-width', '1px')
            .style('opacity', 0);
        
        // to stop the tooltip jittering
        // create highlight cirles
        const highlightPoints = {};
        Object.keys(eventColors).forEach(metric => {
            if (metric !== 'middleclicks') {
                highlightPoints[metric] = svg.append('circle')
                    .attr('r', 5)
                    .attr('fill', eventColors[metric])
                    .attr('stroke', '#000')
                    .attr('stroke-width', 1.5)
                    .style('opacity', 0);
            }
        });

        let lastI = -1; // track last highlighted point index
        let debounceTimeout;
        
        overlay.on('mousemove', function(event) {
            const [mouseX] = d3.pointer(event);

            // find closest data point using bisector
            const xDate = x.invert(mouseX);
            const bisect = d3.bisector(d => d.timestamp).left;
            const i = bisect(data, xDate, 1);
            // edge case
            if (i <= 0 || i >= data.length) return;
            // find which point is closer
            const d0 = data[i - 1];
            const d1 = data[i];
            const d = xDate - d0.timestamp > d1.timestamp - xDate ? d1 : d0;
            const idx = data.indexOf(d);

            // only update content if data point changed
            if (idx !== lastI) {
                lastI = idx;
            
                verticalLine
                    .attr('x1', x(d.timestamp))
                    .attr('x2', x(d.timestamp))
                    .style('opacity', 0.7);

                // update highlight circles
                Object.keys(highlightPoints).forEach(metric => {
                    highlightPoints[metric]
                        .attr('cx', x(d.timestamp))
                        .attr('cy', y(d[metric] || 0))
                        .style('opacity', 1);
                });

                // format
                const timeStr = d.timestamp.toLocaleTimeString([], {
                    hour: '2-digit',
                    minute: '2-digit'
                });
                tooltip.style('opacity', 0.9)
                    .html(`
                        <div style="font-weight:bold;margin-bottom:5px;border-bottom:1px solid #0f0;">
                        ${timeStr}
                        </div>
                        <div>keypresses: ${d.keypresses}</div>
                        <div>mouse moves: ${d.mousemoves.toFixed(2)}m</div>
                        <div>left clicks: ${d.leftclicks}</div>
                        <div>right clicks: ${d.rightclicks}</div>
                    `)
                    .style('left', `${event.pageX + 15}px`)
                    .style('top', `${event.pageY - 10}px`);
                } else {
                    tooltip
                        .style('left', `${event.pageX + 15}px`)
                        .style('top', `${event.pageY - 10}px`);
                }
            });

        overlay.on('mouseout', () => {
            // small delay to prevent jitter
            if (debounceTimeout) clearTimeout(debounceTimeout);
            debounceTimeout = setTimeout(() => {
                tooltip.style('opacity', 0);
                verticalLine.style('opacity', 0);

                // hide all highlight points
                Object.values(highlightPoints).forEach(point => {
                    point.style('opacity', 0);
                });
                lastI = -1;
            }, 100);
        });
    }

    // initialize chart components
    createGrid();
    createAxes();
    drawDataLines();
    createMouseTracking();
}

// call loadData when window is resized
window.addEventListener('resize', () => {
    clearTimeout(window.resizeTimer);
    window.resizeTimer = setTimeout(() => {
        renderChart(currentData);
    }, 250);
});

